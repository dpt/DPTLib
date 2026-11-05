/* crt-gles.c -- CRT post-effect, WebGL backend */

/* SDL's GPU API has no browser driver, so this draws with GLES2, which
 * Emscripten maps to WebGL. */

#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include <GLES2/gl2.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "sdl/crt.h"

/* ----------------------------------------------------------------------- */

struct crt
{
  crt_format_t  format;
  SDL_GLContext context;
  GLuint        program;
  GLuint        buffer;
  GLuint        texture; /* holds the uploaded pixels */
  GLint         size;    /* location of the size uniform */
  GLint         knobs;   /* location of the k uniform */
  int           width;   /* size texture was made at */
  int           height;
};

/* Must match the k[] names below field-for-field. */
typedef struct crt_uniforms
{
  crt_params_t params;
  float        time;
}
crt_uniforms_t;

/* ----------------------------------------------------------------------- */

/* Fullscreen triangle, in clip space. GLES2 has no gl_VertexID to build it in
 * the shader. */
static const GLfloat crt_triangle[] = { -1, -1, 3, -1, -1, 3 };

static const char crt_vertex_glsl[] =
  "attribute vec2 pos;\n"
  "varying vec2 v_uv;\n"
  "void main() {\n"
  "  v_uv = vec2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);\n"
  "  gl_Position = vec4(pos, 0.0, 1.0);\n"
  "}\n";

/* WebGL has no BGRA upload, so all bytes arrive as RGBA and a SWIZZLE define
 * puts them right. Keep in step with the MSL in crt-gpu.c and shaders/crt.frag.
 */
static const char crt_fragment_glsl[] =
  "#extension GL_OES_standard_derivatives : enable\n"
  /* the scanline phase grows with the row count: too much for mediump */
  "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
  "precision highp float;\n"
  "#else\n"
  "precision mediump float;\n"
  "#endif\n"
  "varying vec2 v_uv;\n"
  "uniform sampler2D tex;\n"
  "uniform vec2 size;\n"
  "uniform float k[11];\n"
  "#define curvature k[0]\n"
  "#define bloomThreshold k[1]\n"
  "#define bloomIntensity k[2]\n"
  "#define brightness k[3]\n"
  "#define contrast k[4]\n"
  "#define saturation k[5]\n"
  "#define scanlineIntensity k[6]\n"
  "#define vignetteStrength k[7]\n"
  "#define chromaBleed k[8]\n"
  "#define glitch k[9]\n"
  "#define time k[10]\n"
  "float hash(float x) {\n"
  "  return fract(sin(x * 12.9898) * 43758.5453);\n"
  "}\n"
  "vec3 tap(vec2 uv) {\n"
  "  return texture2D(tex, clamp(uv, 0.0, 1.0)).SWIZZLE;\n"
  "}\n"
  "void main() {\n"
  /* barrel distortion via dot(coord,coord) radial distance */
  "  vec2 coord = v_uv * 2.0 - 1.0;\n"
  "  coord *= 1.0 + dot(coord, coord) * curvature;\n"
  "  vec2 uv = coord * 0.5 + 0.5;\n"
  "  vec2 texel = 1.0 / size;\n"
  /* line tear: seed each source row separately, reseed at 50Hz and shift the
   * rows whose seed clears the threshold, about one in ten.
   *
   * Glitch is a runtime uniform, so the compiler cannot fold hash() * glitch
   * to a constant. mediump sin() on mobile GPUs can return NaN out of range,
   * NaN survives being multiplied by zero, and it turns the whole canvas black
   * on tile-based renderers. This uniform branch keeps the hash from running
   * at all when glitch is off: do not remove it. */
  "  float tick = mod(floor(time * 50.0), 2048.0);\n"
  "  if (glitch > 0.0) {\n"
  "    float band = floor(uv.y / texel.y);\n"
  "    float seed = hash(band * 78.233 + tick * 37.719);\n"
  "    uv.x += (hash(seed * 91.0) - 0.5) * 0.005 * glitch * step(0.9, seed);\n"
  "  }\n"
  /* soft edge: smoothstep border fade rather than a jagged hard cutoff */
  "  vec2 edge = smoothstep(vec2(0.0), vec2(0.005), uv) *\n"
  "              smoothstep(vec2(0.0), vec2(0.005), 1.0 - uv);\n"
  "  float edgeMask = edge.x * edge.y;\n"
  "  vec3 c = tap(uv);\n"
  /* PAL colour bleed: chroma smears rightwards over four decaying taps,
   * luma from the centre tap only */
  "  vec3 W = vec3(0.299, 0.587, 0.114);\n"
  "  vec3 bleed = c * 0.4;\n"
  "  bleed += tap(uv - vec2(texel.x, 0.0)) * 0.3;\n"
  "  bleed += tap(uv - vec2(texel.x * 2.0, 0.0)) * 0.2;\n"
  "  bleed += tap(uv - vec2(texel.x * 3.0, 0.0)) * 0.1;\n"
  "  float ylum = dot(c, W);\n"
  "  vec3 chroma = mix(c - ylum, bleed - dot(bleed, W), chromaBleed);\n"
  "  c = ylum + chroma;\n"
  "  c *= edgeMask;\n"
  /* bloom: threshold-gated centre + 4-tap cross */
  "  vec3 bloom = vec3(0.0);\n"
  "  vec3 bc = c;\n"
  "  vec3 bn = tap(uv + vec2(0.0, texel.y));\n"
  "  vec3 bs = tap(uv - vec2(0.0, texel.y));\n"
  "  vec3 be = tap(uv + vec2(texel.x, 0.0));\n"
  "  vec3 bw = tap(uv - vec2(texel.x, 0.0));\n"
  "  if (max(bc.r, max(bc.g, bc.b)) > bloomThreshold) bloom += bc;\n"
  "  if (max(bn.r, max(bn.g, bn.b)) > bloomThreshold) bloom += bn;\n"
  "  if (max(bs.r, max(bs.g, bs.b)) > bloomThreshold) bloom += bs;\n"
  "  if (max(be.r, max(be.g, be.b)) > bloomThreshold) bloom += be;\n"
  "  if (max(bw.r, max(bw.g, bw.b)) > bloomThreshold) bloom += bw;\n"
  "  c += bloom * bloomIntensity;\n"
  /* brightness / contrast / saturation */
  "  c = (c - 0.5) * contrast + 0.5;\n"
  /* mains flicker: brightness wobble reseeded at 50Hz */
  "  float flicker = 1.0;\n"
  "  if (glitch > 0.0)\n"
  "    flicker = 1.0 + (hash(tick * 91.7) - 0.5) * 0.06 * glitch;\n"
  "  c *= brightness * flicker;\n"
  "  float lum = dot(c, W);\n"
  "  c = mix(vec3(lum), c, saturation);\n"
  /* one scanline per source row, intensity adapted to local luminance;
   * band-limited via fwidth so it fades out rather than aliasing into moire
   * when a device pixel spans several source rows */
  "  float scanPhase = uv.y * size.y * 2.0 * 3.14159265;\n"
  "  float scanWidth = fwidth(scanPhase);\n"
  "  float scanAtten = scanWidth > 0.0001 ?\n"
  "    clamp(sin(scanWidth * 0.5) / (scanWidth * 0.5), 0.0, 1.0) : 1.0;\n"
  "  float scan = sin(scanPhase) * scanAtten * 0.5 + 0.5;\n"
  "  float adaptive = mix(scanlineIntensity, scanlineIntensity * (1.0 - lum), 0.5);\n"
  "  c *= 1.0 - adaptive * scan;\n"
  /* vignette: Chebyshev (max-component) distance falloff */
  "  vec2 d = abs(uv - 0.5) * 2.0;\n"
  "  c *= 1.0 - max(d.x, d.y) * max(d.x, d.y) * vignetteStrength;\n"
  "  gl_FragColor = vec4(c, 1.0);\n"
  "}\n";

/* ----------------------------------------------------------------------- */

/* Set when the browser restores a lost context. The objects below do not
 * survive the loss though the context handle does, so the next crt_render
 * remakes them. Mobile Chrome drops contexts readily under memory pressure:
 * without this the canvas stays black. */
static volatile int crt_needs_remake = 0;

EMSCRIPTEN_KEEPALIVE
void crt__context_lost(void)
{
  fprintf(stderr, "WebGL context lost\n");
}

EMSCRIPTEN_KEEPALIVE
void crt__context_restored(void)
{
  fprintf(stderr, "WebGL context restored\n");
  crt_needs_remake = 1;
}

EM_JS(void, crt_install_context_handlers, (), {
  var canvas = Module['canvas'];
  if (!canvas || canvas.__crtContextHandlers)
    return;
  canvas.__crtContextHandlers = true;
  canvas.addEventListener('webglcontextlost', function(e) {
    e.preventDefault(); /* otherwise the browser never restores it */
    Module['_crt__context_lost']();
  }, false);
  canvas.addEventListener('webglcontextrestored', function() {
    Module['_crt__context_restored']();
  }, false);
});

/* ----------------------------------------------------------------------- */

static void crt_attach_shader(GLuint      program,
                              GLenum      type,
                              const char *define,
                              const char *code)
{
  const char *sources[2];
  GLuint      shader;
  GLint       compiled;
  char        log[512];

  sources[0] = define;
  sources[1] = code;

  shader = glCreateShader(type);
  glShaderSource(shader, 2, sources, NULL);
  glCompileShader(shader);

  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled)
  {
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "Error: CRT shader: %s\n", log);
  }

  /* a failed shader fails the link, which the caller checks */
  glAttachShader(program, shader);
  glDeleteShader(shader); /* only flagged: the program holds it */
}

/* Make the GL objects and set the state they need. */
static bool crt_make_objects(crt_t *crt)
{
  const char *define;
  GLint       linked;

  define = (crt->format == crt_FORMAT_BGRX8888) ? "#define SWIZZLE bgr\n"
                                                : "#define SWIZZLE rgb\n";

  /* the shader's fwidth needs this, and a restored context has lost it */
  emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),
                                    "OES_standard_derivatives");

  crt->program = glCreateProgram();
  crt_attach_shader(crt->program, GL_VERTEX_SHADER, "", crt_vertex_glsl);
  crt_attach_shader(crt->program, GL_FRAGMENT_SHADER, define, crt_fragment_glsl);
  glBindAttribLocation(crt->program, 0, "pos");
  glLinkProgram(crt->program);

  glGetProgramiv(crt->program, GL_LINK_STATUS, &linked);
  if (!linked)
  {
    fprintf(stderr, "Error: CRT program failed to link\n");
    return false;
  }

  glUseProgram(crt->program);
  crt->size  = glGetUniformLocation(crt->program, "size");
  crt->knobs = glGetUniformLocation(crt->program, "k");
  glUniform1i(glGetUniformLocation(crt->program, "tex"), 0);

  glGenBuffers(1, &crt->buffer);
  glBindBuffer(GL_ARRAY_BUFFER, crt->buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(crt_triangle), crt_triangle,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(0);

  /* linear filtering: the soft look is part of the effect */
  glGenTextures(1, &crt->texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, crt->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  crt->width  = 0;
  crt->height = 0;

  /* A canvas has one WebGL context for life: each SDL context wraps the same
   * one, so it arrives with any SDL_Renderer's state. Clear what would get in
   * the way: that renderer's other attributes are 1 and 2. */
  glDisable(GL_BLEND);
  glDisable(GL_SCISSOR_TEST);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  return true;
}

/* ----------------------------------------------------------------------- */

crt_t *crt_create(SDL_Window *window, crt_format_t format)
{
  crt_t *crt;

  crt = calloc(1, sizeof(*crt));
  if (crt == NULL)
    return NULL;

  crt->format = format;

  crt->context = SDL_GL_CreateContext(window);
  if (crt->context == NULL)
  {
    fprintf(stderr, "Error: SDL_GL_CreateContext: %s\n", SDL_GetError());
    free(crt);
    return NULL;
  }

  crt_install_context_handlers();
  crt_needs_remake = 0;

  if (!crt_make_objects(crt))
  {
    crt_destroy(crt, window);
    return NULL;
  }

  return crt;
}

void crt_render(crt_t              *crt,
                SDL_Window         *window,
                const void         *pixels,
                int                 width,
                int                 height,
                const SDL_Rect     *viewport,
                const crt_params_t *params)
{
  crt_uniforms_t uniforms;
  int            win_w, win_h;
  int            sc_w, sc_h;

  SDL_GL_MakeCurrent(window, crt->context);

  if (crt_needs_remake)
  {
    crt_needs_remake = 0;
    if (!crt_make_objects(crt))
      fprintf(stderr, "Error: CRT objects not remade after context restore\n");
  }

  /* ponytail: uploads the whole frame each present, ignoring what changed; a
   * partial upload would save bandwidth if it ever matters */
  if (width != crt->width || height != crt->height)
  {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);
    glUniform2f(crt->size, (GLfloat) width, (GLfloat) height);
    crt->width  = width;
    crt->height = height;
  }
  else
  {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
  }

  uniforms.params = *params;
  uniforms.time   = SDL_GetTicks() * 0.001f;
  glUniform1fv(crt->knobs, sizeof(uniforms) / sizeof(float),
               (const GLfloat *) &uniforms);

  glClear(GL_COLOR_BUFFER_BIT);

  /* the canvas may be denser than the window on high-DPI displays, and GL's
   * origin is bottom left */
  SDL_GetWindowSize(window, &win_w, &win_h);
  SDL_GetWindowSizeInPixels(window, &sc_w, &sc_h);
  glViewport(viewport->x * sc_w / win_w,
             sc_h - (viewport->y + viewport->h) * sc_h / win_h,
             viewport->w * sc_w / win_w,
             viewport->h * sc_h / win_h);

  glDrawArrays(GL_TRIANGLES, 0, 3);

  SDL_GL_SwapWindow(window);
}

void crt_destroy(crt_t *crt, SDL_Window *window)
{
  (void) window;

  if (crt == NULL)
    return;

  glDeleteTextures(1, &crt->texture);
  glDeleteBuffers(1, &crt->buffer);
  glDeleteProgram(crt->program);
  SDL_GL_DestroyContext(crt->context);
  free(crt);
}
