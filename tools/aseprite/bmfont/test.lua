-- test.lua -- headless check for bmfont.lua: grow every font's cells, check
-- the new geometry, shrink back and expect the original pixels on every
-- layer. Covers .png and layered .aseprite fonts.
--
--   aseprite -b --script-param dir=resources/bmfonts \
--            --script tools/aseprite/bmfont/test.lua

local bm = dofile(app.fs.joinPath(app.fs.filePath(debug.getinfo(1, "S").source:sub(2)),
                                  "bmfont.lua"))
local dir = app.params.dir
local grow   = { left = 1, right = 2, top = 3, bottom = 1 }
local shrink = { left = -1, right = -2, top = -3, bottom = -1 }
local failed = 0

local function getter(img)
  return function(x, y) return img:getPixel(x, y) end
end

for _, name in ipairs(app.fs.listFiles(dir)) do
  if name:match("%.png$") or name:match("%.aseprite$") then
    local sprite = app.open(app.fs.joinPath(dir, name))
    local m, err = bm.measure(sprite, 1)
    if not m then
      print("SKIP " .. name .. ": " .. err)
    else
      -- grown sheet, flattened for geometry checks
      local bigs = {}
      for i, l in ipairs(m.layers) do
        bigs[i] = bm.resize_image(m, grow, l.get, l == m.grid_layer)
      end
      local function flat(x, y)
        for i = #bigs, 1, -1 do
          local v = bigs[i]:getPixel(x, y)
          if v ~= 0 then return v end
        end
        return 0
      end
      local mb = bm.geometry(flat, bigs[1].width, bigs[1].height)
      local ok = mb and mb.cw == m.cw + 3 and mb.ch == m.ch + 4
      if ok and m.baseline then
        ok = mb.baseline == m.baseline + 3 and mb.bottom == mb.ch - 1
      end
      for i, l in ipairs(m.layers) do
        if not ok then break end
        local back = bm.resize_image(mb, shrink, getter(bigs[i]),
                                     l == m.grid_layer)
        for y = 0, m.h - 1 do
          for x = 0, m.w - 1 do
            if back:getPixel(x, y) ~= l.get(x, y) then ok = false end
          end
        end
      end
      print((ok and "ok   " or "FAIL ") .. name .. " (" .. #m.layers ..
            " layers, cell " .. m.cw .. "x" .. m.ch ..
            (m.baseline and "" or ", no grid") .. ")")
      if not ok then failed = failed + 1 end
    end
    sprite:close()
  end
end
print(failed == 0 and "all passed" or failed .. " failed")
