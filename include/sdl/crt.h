/* crt.h -- CRT post-effect for SDL3 windows */

/**
 * \file crt.h
 *
 * Presents a block of pixels through a CRT fragment shader: curvature,
 * chroma bleed, bloom, scanlines, vignette, plus an optional mains flicker
 * and line tear.
 *
 * Under Emscripten it draws with WebGL. Elsewhere it draws with SDL's GPU
 * API, using Metal on Apple platforms and Vulkan on others where it was
 * built with glslangValidator. DPTLIB_CRT_AVAILABLE is defined for code
 * linking this when one of those applies. Otherwise crt_create fails.
 *
 * Owns the window's swapchain or GL context while it lives: an SDL_Renderer
 * must not exist on the same window.
 *
 * Links SDL3 alone, not the rest of DPTLib.
 */

#ifndef DPTLIB_SDL_CRT_H
#define DPTLIB_SDL_CRT_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A CRT effect bound to a window. */
typedef struct crt crt_t;

/** Pixel layouts, naming the bytes in memory order. The last is ignored. */
typedef enum crt_format
{
  crt_FORMAT_RGBX8888,
  crt_FORMAT_BGRX8888
}
crt_format_t;

/**
 * Shader knobs. All floats: the backends pass the struct on as it stands.
 */
typedef struct crt_params
{
  float curvature;          /**< barrel distortion strength */
  float bloom_threshold;    /**< level above which bloom kicks in */
  float bloom_intensity;    /**< bloom contribution scale */
  float brightness;         /**< post-contrast multiplicative brightness */
  float contrast;           /**< contrast around mid-grey */
  float saturation;         /**< 1.0 = full colour, 0.0 = greyscale */
  float scanline_intensity; /**< base scanline darkening amount */
  float vignette_strength;  /**< corner darkening strength */
  float chroma_bleed;       /**< PAL horizontal colour smear, 0.0 = off */
  float glitch;             /**< mains flicker and line tear, 0.0 = off */
}
crt_params_t;

/**
 * Claim a window and build the shader pipeline.
 *
 * Under Emscripten the window must already be an OpenGL one, as creating an
 * SDL_Renderer on it makes it.
 *
 * \param[in] window Window to claim.
 * \param[in] format Layout of the pixels which crt_render will be given.
 *
 * \return The new effect, or NULL on failure, which is reported on stderr.
 */
crt_t *crt_create(SDL_Window *window, crt_format_t format);

/**
 * Upload pixels and present one frame through the shader.
 *
 * The source texture is remade whenever the size changes.
 *
 * \param[in] crt      Effect from crt_create.
 * \param[in] window   Window it was created with.
 * \param[in] pixels   width x height pixels, tightly packed, top row first.
 * \param[in] width    Width of pixels.
 * \param[in] height   Height of pixels.
 * \param[in] viewport Where to draw, in window units with a top left origin.
 * \param[in] params   Shader knobs for this frame.
 */
void crt_render(crt_t              *crt,
                SDL_Window         *window,
                const void         *pixels,
                int                 width,
                int                 height,
                const SDL_Rect     *viewport,
                const crt_params_t *params);

/**
 * Release everything crt_create made and hand the window back.
 *
 * \param[in] crt    Effect from crt_create. May be NULL.
 * \param[in] window Window it was created with.
 */
void crt_destroy(crt_t *crt, SDL_Window *window);

#ifdef __cplusplus
}
#endif

#endif /* DPTLIB_SDL_CRT_H */
