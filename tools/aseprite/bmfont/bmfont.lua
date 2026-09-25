-- bmfont.lua -- Aseprite extension: resize the glyph cells of a DPTLib bmfont
--
-- A bmfont PNG (docs/framebuf/bmfont.md) is 32 cells per row. Each cell's top
-- row is the advance-width strip (index 2 pixels, counted by the loader); the
-- rest is the glyph body (index 1 ink). Index 3 draws the grid: a left
-- sidebearing line plus full-width baseline and cell-bottom rows, which the
-- loader reads back from the first inkless cell for ascent and descent.
--
-- Aseprite's Canvas Size would resize the whole sheet; this instead grows or
-- shrinks every cell by a number of pixels on each edge, keeping the glyphs in
-- place, adjusting advances for left-edge changes, and redrawing the grid so
-- the baseline follows the glyphs and the bottom row stays at the cell bottom.
-- Glyph IDs are unchanged, so any .map sidecar remains valid.
--
-- Install: (cd tools/aseprite/bmfont && zip ../bmfont.aseprite-extension \
--          package.json bmfont.lua), then double-click the result. The
-- command appears as Sprite > Resize bmfont Cells...

local CHARS_PER_ROW = 32

local IDX_BG   = 0
local IDX_INK  = 1
local IDX_ADW  = 2
local IDX_GRID = 3

-- Pixel at sprite coordinates, treating anything outside the cel as
-- background (cels on transparent layers are trimmed).
local function reader(cel)
  local img = cel.image
  local px, py = cel.position.x, cel.position.y
  return function(x, y)
    local ix, iy = x - px, y - py
    if ix < 0 or iy < 0 or ix >= img.width or iy >= img.height then
      return IDX_BG
    end
    return img:getPixel(ix, iy)
  end
end

-- Mirrors detect_gridheight() in libraries/framebuf/bmfont/bmfont.c.
local function detect_cell_height(get, w, h)
  local cw = w // CHARS_PER_ROW
  local has_adw, has_ink = {}, {}
  for y = 0, h - 1 do
    for x = 0, w - 1 do
      local v = get(x, y)
      if v == IDX_ADW then has_adw[y] = true end
      if v == IDX_INK then has_ink[y] = true end
    end
  end
  for ch = 2, math.min(cw * 3, h) do
    if h % ch == 0 then
      local ok = true
      for y = 0, h - 1 do
        if y % ch == 0 then
          ok = has_adw[y] and not has_ink[y]
        else
          ok = not has_adw[y]
        end
        if not ok then break end
      end
      if ok then return ch end
    end
  end
  return nil
end

-- Mirrors detect_baseline_metrics(): returns the in-cell rows of the baseline
-- and cell bottom plus the cell's origin, or nil if the font has no grid rows.
local function detect_grid_rows(get, w, h, cw, ch)
  for y0 = 0, h - 1, ch do
    for x0 = 0, w - cw, cw do
      local inky = false
      for y = y0, y0 + ch - 1 do
        for x = x0, x0 + cw - 1 do
          if get(x, y) == IDX_INK then inky = true break end
        end
        if inky then break end
      end
      if not inky then
        local rows = {}
        for y = 1, ch - 1 do
          local full = true
          for x = x0, x0 + cw - 1 do
            if get(x, y0 + y) ~= IDX_GRID then full = false break end
          end
          if full then
            rows[#rows + 1] = y
            if #rows == 2 then return rows[1], rows[2], x0, y0 end
          end
        end
        return nil
      end
    end
  end
  return nil
end

-- Font geometry from a pixel getter. Returns a table or nil, message.
local function geometry(get, w, h)
  if w % CHARS_PER_ROW ~= 0 then
    return nil, "Width " .. w .. " is not a multiple of 32 cells."
  end
  local cw = w // CHARS_PER_ROW
  local ch = detect_cell_height(get, w, h)
  if not ch then
    return nil, "Couldn't detect the cell height from the advance strips."
  end
  local baseline, bottom, gx, gy = detect_grid_rows(get, w, h, cw, ch)
  return { get = get, w = w, h = h, cw = cw, ch = ch,
           baseline = baseline, bottom = bottom, gx = gx, gy = gy }
end

-- Image layers, bottom to top, descending into groups.
local function image_layers(layers, out)
  for _, layer in ipairs(layers) do
    if layer.isGroup then
      image_layers(layer.layers, out)
    elseif layer.isImage then
      out[#out + 1] = layer
    end
  end
  return out
end

-- Measure a sprite's font geometry from its visible layers composited, since
-- the ink, strips and grid may each live on their own layer. Returns a table
-- (with the per-layer getters) or nil, message.
local function measure(sprite, frame)
  if sprite.colorMode ~= ColorMode.INDEXED then
    return nil, "Sprite must be in Indexed colour mode."
  end

  local layers = {}
  for _, layer in ipairs(image_layers(sprite.layers, {})) do
    local cel = layer:cel(frame)
    if cel then
      layers[#layers + 1] = { layer = layer, cel = cel, get = reader(cel) }
    end
  end

  -- topmost visible non-background index wins
  local function composite(x, y)
    for i = #layers, 1, -1 do
      if layers[i].layer.isVisible then
        local v = layers[i].get(x, y)
        if v ~= IDX_BG then return v end
      end
    end
    return IDX_BG
  end

  local m, err = geometry(composite, sprite.width, sprite.height)
  if not m then return nil, err end
  m.layers = layers

  -- the standard grid is redrawn on the layer that holds it
  if m.baseline then
    for i = #layers, 1, -1 do
      if layers[i].layer.isVisible and
         layers[i].get(m.gx, m.gy + m.baseline) == IDX_GRID then
        m.grid_layer = layers[i]
        break
      end
    end
  end

  return m
end

-- Build one layer's resized sheet, reading pixels through get. m is the
-- font geometry; d = { left, right, top, bottom } in pixels, negative values
-- cropping that edge. draw_grid redraws the standard grid on this layer.
local function resize_image(m, d, get, draw_grid)
  local cw, ch = m.cw, m.ch
  local ncw = cw + d.left + d.right
  local nch = ch + d.top + d.bottom
  local nrows = m.h // ch
  local img = Image(ImageSpec{ width = ncw * CHARS_PER_ROW,
                               height = nch * nrows,
                               colorMode = ColorMode.INDEXED,
                               transparentColor = IDX_BG })
  img:clear(IDX_BG)

  -- the standard grid is redrawn when present, following the new baseline
  local nbaseline = m.baseline and m.baseline + d.top
  local regrid = nbaseline and nbaseline >= 1 and nbaseline < nch - 1

  for r = 0, nrows - 1 do
    for c = 0, CHARS_PER_ROW - 1 do
      local ox, oy = c * cw, r * ch
      local nx, ny = c * ncw, r * nch

      local has_adv = false
      for x = 0, cw - 1 do
        if get(ox + x, oy) == IDX_ADW then has_adv = true break end
      end
      -- a grid tool may run the left line up through an empty strip
      local strip_grid = not has_adv and get(ox, oy) == IDX_GRID

      -- strip and body shift with the glyph; the strip stays on row 0. When
      -- regridding, only the standard grid pixels (left line, baseline and
      -- bottom rows) are dropped -- other index 3 marks are the author's.
      for y = 0, nch - 1 do
        local sy = (y == 0) and 0 or y - d.top
        if (y == 0 or sy >= 1) and sy < ch then
          for x = 0, ncw - 1 do
            local sx = x - d.left
            if sx >= 0 and sx < cw then
              local v = get(ox + sx, oy + sy)
              if regrid and v == IDX_GRID and
                 ((sx == 0 and (sy > 0 or strip_grid)) or
                  sy == m.baseline or sy == m.bottom) then
                v = IDX_BG
              end
              if v ~= IDX_BG then img:drawPixel(nx + x, ny + y, v) end
            end
          end
        end
      end

      -- the loader counts strip pixels, so padding the left edge extends the
      -- advance by the same amount; an empty strip (zero-advance or unused
      -- cell) stays empty
      if has_adv then
        for x = 0, d.left - 1 do img:drawPixel(nx + x, ny, IDX_ADW) end
      end

      -- grid under the ink: left sidebearing, baseline and cell bottom
      if regrid and draw_grid then
        local function grid(x, y)
          if img:getPixel(nx + x, ny + y) == IDX_BG then
            img:drawPixel(nx + x, ny + y, IDX_GRID)
          end
        end
        for y = strip_grid and 0 or 1, nch - 1 do grid(0, y) end
        for x = 0, ncw - 1 do
          grid(x, nbaseline)
          grid(x, nch - 1)
        end
      end
    end
  end

  return img
end

local function run()
  local sprite = app.sprite
  local frame = app.frame
  if not sprite or not frame then
    app.alert("Open a bmfont PNG first.")
    return
  end

  local m, err = measure(sprite, frame)
  if not m then
    app.alert{ title = "Resize bmfont Cells", text = err }
    return
  end

  local info = string.format("Cell %d x %d", m.cw, m.ch)
  if m.baseline then
    info = info .. string.format(", ascent %d, descent %d",
                                 m.baseline - 1, m.bottom - m.baseline)
  else
    info = info .. ", no grid rows"
  end

  local dlg = Dialog("Resize bmfont Cells")
  dlg:label{ text = info }
  dlg:separator{ text = "Pixels to add (negative crops)" }
  dlg:number{ id = "top",    label = "Top",    text = "0", decimals = 0 }
  dlg:number{ id = "bottom", label = "Bottom", text = "0", decimals = 0 }
  dlg:number{ id = "left",   label = "Left",   text = "0", decimals = 0 }
  dlg:number{ id = "right",  label = "Right",  text = "0", decimals = 0 }
  dlg:button{ id = "ok", text = "OK", focus = true }
  dlg:button{ id = "cancel", text = "Cancel" }
  dlg:show()

  local d = dlg.data
  if not d.ok then return end

  local ncw = m.cw + d.left + d.right
  local nch = m.ch + d.top + d.bottom
  -- the loader keeps each cell's pixels in a 64-bit word: 32 px max
  if ncw < 1 or ncw > 32 or nch < 2 then
    app.alert("New cell size " .. ncw .. " x " .. nch ..
              " is out of range (width 1..32, height 2+).")
    return
  end

  local imgs = {}
  for i, l in ipairs(m.layers) do
    imgs[i] = resize_image(m, d, l.get, l == m.grid_layer)
  end
  app.transaction("Resize bmfont Cells", function()
    sprite:crop(0, 0, imgs[1].width, imgs[1].height)
    for i, l in ipairs(m.layers) do
      l.cel.image = imgs[i]
      l.cel.position = Point(0, 0)
    end
  end)
  app.refresh()
end

function init(plugin)
  plugin:newCommand{
    id = "DPTLibBmfontResizeCells",
    title = "Resize bmfont Cells...",
    group = "sprite_size",
    onclick = run,
    onenabled = function() return app.sprite ~= nil end
  }
end

function exit(plugin)
end

-- exposed for headless testing (aseprite -b --script)
return { geometry = geometry, measure = measure,
         resize_image = resize_image }
