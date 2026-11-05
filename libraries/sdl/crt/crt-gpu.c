/* crt-gpu.c -- CRT post-effect, SDL GPU backend */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "sdl/crt.h"

/* ----------------------------------------------------------------------- */

struct crt
{
  crt_format_t             format;
  SDL_GPUDevice           *gpu;
  SDL_GPUTexture          *texture;  /* holds the uploaded pixels */
  SDL_GPUTransferBuffer   *transfer; /* staging buffer for the upload */
  int                      width;    /* size texture/transfer were made at */
  int                      height;
  SDL_GPUSampler          *sampler;
  SDL_GPUGraphicsPipeline *pipeline;
};

/* Must match the MSL and SPIR-V Params structs field-for-field: plain floats,
 * same order, no padding. */
typedef struct crt_uniforms
{
  crt_params_t params;
  float        time;
}
crt_uniforms_t;

/* ----------------------------------------------------------------------- */

#if defined(__APPLE__)

/* MSL is compiled at runtime by Metal, so there's no offline build step.
 * Fullscreen triangle generated from vertex_id, so no vertex buffer. */
static const char crt_vertex_msl[] =
  "#include <metal_stdlib>\n"
  "using namespace metal;\n"
  "struct VSOut { float4 position [[position]]; float2 uv; };\n"
  "vertex VSOut vs_main(uint vid [[vertex_id]]) {\n"
  "  float2 pos[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };\n"
  "  float2 uv[3]  = { float2(0,1),  float2(2,1),  float2(0,-1) };\n"
  "  VSOut out;\n"
  "  out.position = float4(pos[vid], 0.0, 1.0);\n"
  "  out.uv = uv[vid];\n"
  "  return out;\n"
  "}\n";

/* Keep in step with the GLSL in crt-gles.c and shaders/crt.frag. */
static const char crt_fragment_msl[] =
  "#include <metal_stdlib>\n"
  "using namespace metal;\n"
  "struct VSOut { float4 position [[position]]; float2 uv; };\n"
  "struct Params {\n"
  "  float curvature;\n"
  "  float bloomThreshold;\n"
  "  float bloomIntensity;\n"
  "  float brightness;\n"
  "  float contrast;\n"
  "  float saturation;\n"
  "  float scanlineIntensity;\n"
  "  float vignetteStrength;\n"
  "  float chromaBleed;\n"
  "  float glitch;\n"
  "  float time;\n"
  "};\n"
  "static inline float hash(float x) {\n"
  "  return fract(sin(x * 12.9898) * 43758.5453);\n"
  "}\n"
  "fragment float4 fs_main(VSOut in [[stage_in]],\n"
  "                        texture2d<float> tex [[texture(0)]],\n"
  "                        sampler samp [[sampler(0)]],\n"
  "                        constant Params& p [[buffer(0)]]) {\n"
  /* barrel distortion via dot(coord,coord) radial distance */
  "  float2 coord = in.uv * 2.0 - 1.0;\n"
  "  coord *= 1.0 + dot(coord, coord) * p.curvature;\n"
  "  float2 uv = coord * 0.5 + 0.5;\n"
  "  float2 size = float2(tex.get_width(), tex.get_height());\n"
  "  float2 texel = 1.0 / size;\n"
  /* line tear: see the GLSL in crt-gles.c on why this branch must stay */
  "  float tick = fmod(floor(p.time * 50.0), 2048.0);\n"
  "  if (p.glitch > 0.0) {\n"
  "    float band = floor(uv.y * size.y);\n"
  "    float seed = hash(band * 78.233 + tick * 37.719);\n"
  "    uv.x += (hash(seed * 91.0) - 0.5) * 0.005 * p.glitch * step(0.9, seed);\n"
  "  }\n"
  /* soft edge: smoothstep border fade rather than a jagged hard cutoff */
  "  float2 edge = smoothstep(float2(0.0), float2(0.005), uv) *\n"
  "                smoothstep(float2(0.0), float2(0.005), 1.0 - uv);\n"
  "  float edgeMask = edge.x * edge.y;\n"
  "  float2 uvc = clamp(uv, 0.0, 1.0);\n"
  "  float4 c = tex.sample(samp, uvc);\n"
  /* PAL colour bleed: chroma smears rightwards over four decaying taps,
   * luma from the centre tap only */
  "  float3 W = float3(0.299, 0.587, 0.114);\n"
  "  float3 bleed = c.rgb * 0.4;\n"
  "  bleed += tex.sample(samp, clamp(uvc - float2(texel.x, 0.0), 0.0, 1.0)).rgb * 0.3;\n"
  "  bleed += tex.sample(samp, clamp(uvc - float2(texel.x * 2.0, 0.0), 0.0, 1.0)).rgb * 0.2;\n"
  "  bleed += tex.sample(samp, clamp(uvc - float2(texel.x * 3.0, 0.0), 0.0, 1.0)).rgb * 0.1;\n"
  "  float ylum = dot(c.rgb, W);\n"
  "  float3 chroma = mix(c.rgb - ylum, bleed - dot(bleed, W), p.chromaBleed);\n"
  "  c.rgb = ylum + chroma;\n"
  "  c *= edgeMask;\n"
  /* bloom: threshold-gated centre + 4-tap cross */
  "  float3 bloom = float3(0.0);\n"
  "  float3 bc = c.rgb;\n"
  "  float3 bn = tex.sample(samp, clamp(uvc + float2(0.0, texel.y), 0.0, 1.0)).rgb;\n"
  "  float3 bs = tex.sample(samp, clamp(uvc - float2(0.0, texel.y), 0.0, 1.0)).rgb;\n"
  "  float3 be = tex.sample(samp, clamp(uvc + float2(texel.x, 0.0), 0.0, 1.0)).rgb;\n"
  "  float3 bw = tex.sample(samp, clamp(uvc - float2(texel.x, 0.0), 0.0, 1.0)).rgb;\n"
  "  if (max(bc.r, max(bc.g, bc.b)) > p.bloomThreshold) bloom += bc;\n"
  "  if (max(bn.r, max(bn.g, bn.b)) > p.bloomThreshold) bloom += bn;\n"
  "  if (max(bs.r, max(bs.g, bs.b)) > p.bloomThreshold) bloom += bs;\n"
  "  if (max(be.r, max(be.g, be.b)) > p.bloomThreshold) bloom += be;\n"
  "  if (max(bw.r, max(bw.g, bw.b)) > p.bloomThreshold) bloom += bw;\n"
  "  c.rgb += bloom * p.bloomIntensity;\n"
  /* brightness / contrast / saturation */
  "  c.rgb = (c.rgb - 0.5) * p.contrast + 0.5;\n"
  /* mains flicker: brightness wobble reseeded at 50Hz */
  "  float flicker = 1.0;\n"
  "  if (p.glitch > 0.0)\n"
  "    flicker = 1.0 + (hash(tick * 91.7) - 0.5) * 0.06 * p.glitch;\n"
  "  c.rgb *= p.brightness * flicker;\n"
  "  float lum = dot(c.rgb, W);\n"
  "  c.rgb = mix(float3(lum), c.rgb, p.saturation);\n"
  /* one scanline per source row, intensity adapted to local luminance;
   * band-limited via fwidth so it fades out rather than aliasing into moire
   * when a device pixel spans several source rows */
  "  float scanPhase = uv.y * size.y * 2.0 * 3.14159265;\n"
  "  float scanWidth = fwidth(scanPhase);\n"
  "  float scanAtten = scanWidth > 0.0001 ? saturate(sin(scanWidth * 0.5) / (scanWidth * 0.5)) : 1.0;\n"
  "  float scan = sin(scanPhase) * scanAtten * 0.5 + 0.5;\n"
  "  float adaptive = mix(p.scanlineIntensity, p.scanlineIntensity * (1.0 - lum), 0.5);\n"
  "  c.rgb *= 1.0 - adaptive * scan;\n"
  /* vignette: Chebyshev (max-component) distance falloff */
  "  float2 d = abs(uv - 0.5) * 2.0;\n"
  "  c.rgb *= 1.0 - max(d.x, d.y) * max(d.x, d.y) * p.vignetteStrength;\n"
  "  return float4(c.rgb, 1.0);\n"
  "}\n";

#define crt_SHADER_FORMAT  SDL_GPU_SHADERFORMAT_MSL
#define crt_VERTEX         crt_vertex_msl
#define crt_VERTEX_SIZE    strlen(crt_vertex_msl)
#define crt_VERTEX_ENTRY   "vs_main"
#define crt_FRAGMENT       crt_fragment_msl
#define crt_FRAGMENT_SIZE  strlen(crt_fragment_msl)
#define crt_FRAGMENT_ENTRY "fs_main"

#elif defined(DPTLIB_CRT_VULKAN)

/* Compiled from shaders/crt.{vert,frag} by glslangValidator at build time. */
#include "crt-spirv.h"

#define crt_SHADER_FORMAT  SDL_GPU_SHADERFORMAT_SPIRV
#define crt_VERTEX         crt_vertex_spirv
#define crt_VERTEX_SIZE    crt_vertex_spirv_len
#define crt_VERTEX_ENTRY   "main"
#define crt_FRAGMENT       crt_fragment_spirv
#define crt_FRAGMENT_SIZE  crt_fragment_spirv_len
#define crt_FRAGMENT_ENTRY "main"

#else

/* No shaders for this platform: SDL_CreateGPUDevice will find no driver, so
 * crt_create fails. */
#define crt_SHADER_FORMAT  SDL_GPU_SHADERFORMAT_INVALID
#define crt_VERTEX         ""
#define crt_VERTEX_SIZE    0
#define crt_VERTEX_ENTRY   ""
#define crt_FRAGMENT       ""
#define crt_FRAGMENT_SIZE  0
#define crt_FRAGMENT_ENTRY ""

#endif

/* ----------------------------------------------------------------------- */

static SDL_GPUShader *crt_shader(SDL_GPUDevice     *gpu,
                                 const void        *code,
                                 size_t             code_size,
                                 const char        *entrypoint,
                                 SDL_GPUShaderStage stage,
                                 Uint32             nsamplers)
{
  SDL_GPUShaderCreateInfo info;

  memset(&info, 0, sizeof(info));
  info.code                = (const Uint8 *) code;
  info.code_size           = code_size;
  info.entrypoint          = entrypoint;
  info.format              = crt_SHADER_FORMAT;
  info.stage               = stage;
  info.num_samplers        = nsamplers;
  info.num_uniform_buffers = nsamplers; /* the fragment stage has one of each */

  return SDL_CreateGPUShader(gpu, &info);
}

crt_t *crt_create(SDL_Window *window, crt_format_t format)
{
  crt_t                            *crt;
  SDL_GPUSamplerCreateInfo          sampler_info;
  SDL_GPUShader                    *vertex_shader;
  SDL_GPUShader                    *fragment_shader;
  SDL_GPUColorTargetDescription     target;
  SDL_GPUGraphicsPipelineCreateInfo pipeline_info;

  crt = calloc(1, sizeof(*crt));
  if (crt == NULL)
    return NULL;

  crt->format = format;

  crt->gpu = SDL_CreateGPUDevice(crt_SHADER_FORMAT, false, NULL);
  if (crt->gpu == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateGPUDevice: %s\n", SDL_GetError());
    free(crt);
    return NULL;
  }

  if (!SDL_ClaimWindowForGPUDevice(crt->gpu, window))
  {
    fprintf(stderr, "Error: SDL_ClaimWindowForGPUDevice: %s\n", SDL_GetError());
    SDL_DestroyGPUDevice(crt->gpu);
    free(crt);
    return NULL;
  }

  /* On Wayland and X11, reclaiming a window straight after the swapchain which
   * last owned it went away presents every frame but the compositor keeps the
   * old surface. Configuring the swapchain explicitly makes it pick up the new
   * one. It's harmless on Metal. See also the note on window recreation in
   * ZXGame's SDLHost.c. */
  if (!SDL_SetGPUSwapchainParameters(crt->gpu, window,
                                     SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                     SDL_GPU_PRESENTMODE_VSYNC))
  {
    fprintf(stderr, "Error: SDL_SetGPUSwapchainParameters: %s\n", SDL_GetError());
    crt_destroy(crt, window);
    return NULL;
  }

  /* linear filtering: the soft look is part of the effect */
  memset(&sampler_info, 0, sizeof(sampler_info));
  sampler_info.min_filter     = SDL_GPU_FILTER_LINEAR;
  sampler_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  crt->sampler = SDL_CreateGPUSampler(crt->gpu, &sampler_info);

  vertex_shader   = crt_shader(crt->gpu, crt_VERTEX, crt_VERTEX_SIZE,
                               crt_VERTEX_ENTRY, SDL_GPU_SHADERSTAGE_VERTEX, 0);
  fragment_shader = crt_shader(crt->gpu, crt_FRAGMENT, crt_FRAGMENT_SIZE,
                               crt_FRAGMENT_ENTRY, SDL_GPU_SHADERSTAGE_FRAGMENT, 1);

  if (crt->sampler != NULL && vertex_shader != NULL && fragment_shader != NULL)
  {
    memset(&target, 0, sizeof(target));
    target.format = SDL_GetGPUSwapchainTextureFormat(crt->gpu, window);

    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.vertex_shader   = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.target_info.color_target_descriptions = &target;
    pipeline_info.target_info.num_color_targets         = 1;

    crt->pipeline = SDL_CreateGPUGraphicsPipeline(crt->gpu, &pipeline_info);
  }

  /* the pipeline keeps what it needs; the modules can go now */
  SDL_ReleaseGPUShader(crt->gpu, vertex_shader);
  SDL_ReleaseGPUShader(crt->gpu, fragment_shader);

  if (crt->pipeline == NULL)
  {
    fprintf(stderr, "Error: CRT pipeline: %s\n", SDL_GetError());
    crt_destroy(crt, window);
    return NULL;
  }

  return crt;
}

/* (Re)make the texture and transfer buffer at width x height. */
static bool crt_set_size(crt_t *crt, int width, int height)
{
  SDL_GPUTextureCreateInfo        texture_info;
  SDL_GPUTransferBufferCreateInfo transfer_info;

  SDL_ReleaseGPUTexture(crt->gpu, crt->texture);
  SDL_ReleaseGPUTransferBuffer(crt->gpu, crt->transfer);
  crt->width  = 0;
  crt->height = 0;

  /* these name their bytes in memory order, as crt_format_t does */
  memset(&texture_info, 0, sizeof(texture_info));
  texture_info.type                 = SDL_GPU_TEXTURETYPE_2D;
  texture_info.format               = (crt->format == crt_FORMAT_BGRX8888) ?
                                      SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM :
                                      SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texture_info.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture_info.width                = width;
  texture_info.height               = height;
  texture_info.layer_count_or_depth = 1;
  texture_info.num_levels           = 1;
  texture_info.sample_count         = SDL_GPU_SAMPLECOUNT_1;

  memset(&transfer_info, 0, sizeof(transfer_info));
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size  = (Uint32) width * height * 4;

  crt->texture  = SDL_CreateGPUTexture(crt->gpu, &texture_info);
  crt->transfer = SDL_CreateGPUTransferBuffer(crt->gpu, &transfer_info);
  if (crt->texture == NULL || crt->transfer == NULL)
  {
    fprintf(stderr, "Error: CRT texture: %s\n", SDL_GetError());
    return false;
  }

  crt->width  = width;
  crt->height = height;
  return true;
}

void crt_render(crt_t              *crt,
                SDL_Window         *window,
                const void         *pixels,
                int                 width,
                int                 height,
                const SDL_Rect     *viewport,
                const crt_params_t *params)
{
  crt_uniforms_t               uniforms;
  void                        *mapped;
  SDL_GPUTextureTransferInfo   src;
  SDL_GPUTextureRegion         dst;
  SDL_GPUCommandBuffer        *cmdbuf;
  SDL_GPUCopyPass             *copy_pass;
  SDL_GPUTexture              *swapchain;
  Uint32                       sc_w, sc_h;
  int                          win_w, win_h;
  SDL_GPUColorTargetInfo       target;
  SDL_GPURenderPass           *render_pass;
  SDL_GPUViewport              gpu_viewport;
  SDL_GPUTextureSamplerBinding binding;

  if ((width != crt->width || height != crt->height) &&
      !crt_set_size(crt, width, height))
    return;

  uniforms.params = *params;
  uniforms.time   = SDL_GetTicks() * 0.001f;

  /* ponytail: uploads the whole frame each present, ignoring what changed; a
   * partial upload would save bandwidth if it ever matters */
  mapped = SDL_MapGPUTransferBuffer(crt->gpu, crt->transfer, true);
  if (mapped == NULL)
    return;

  memcpy(mapped, pixels, (size_t) width * height * 4);
  SDL_UnmapGPUTransferBuffer(crt->gpu, crt->transfer);

  memset(&src, 0, sizeof(src));
  src.transfer_buffer = crt->transfer;
  src.pixels_per_row  = width;
  src.rows_per_layer  = height;

  memset(&dst, 0, sizeof(dst));
  dst.texture = crt->texture;
  dst.w       = width;
  dst.h       = height;
  dst.d       = 1;

  cmdbuf    = SDL_AcquireGPUCommandBuffer(crt->gpu);
  copy_pass = SDL_BeginGPUCopyPass(cmdbuf);
  SDL_UploadToGPUTexture(copy_pass, &src, &dst, true);
  SDL_EndGPUCopyPass(copy_pass);

  if (SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, window, &swapchain,
                                            &sc_w, &sc_h) &&
      swapchain != NULL)
  {
    SDL_GetWindowSize(window, &win_w, &win_h);

    memset(&target, 0, sizeof(target));
    target.texture       = swapchain;
    target.load_op       = SDL_GPU_LOADOP_CLEAR;
    target.store_op      = SDL_GPU_STOREOP_STORE;
    target.clear_color.a = 1.0f;

    render_pass = SDL_BeginGPURenderPass(cmdbuf, &target, 1, NULL);

    /* the swapchain may be denser than the window on high-DPI displays */
    gpu_viewport.x         = (float) viewport->x * sc_w / win_w;
    gpu_viewport.y         = (float) viewport->y * sc_h / win_h;
    gpu_viewport.w         = (float) viewport->w * sc_w / win_w;
    gpu_viewport.h         = (float) viewport->h * sc_h / win_h;
    gpu_viewport.min_depth = 0.0f;
    gpu_viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(render_pass, &gpu_viewport);

    binding.texture = crt->texture;
    binding.sampler = crt->sampler;

    SDL_BindGPUGraphicsPipeline(render_pass, crt->pipeline);
    SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
    SDL_PushGPUFragmentUniformData(cmdbuf, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);

    SDL_EndGPURenderPass(render_pass);
  }

  SDL_SubmitGPUCommandBuffer(cmdbuf);
}

void crt_destroy(crt_t *crt, SDL_Window *window)
{
  if (crt == NULL)
    return;

  SDL_ReleaseGPUGraphicsPipeline(crt->gpu, crt->pipeline);
  SDL_ReleaseGPUSampler(crt->gpu, crt->sampler);
  SDL_ReleaseGPUTransferBuffer(crt->gpu, crt->transfer);
  SDL_ReleaseGPUTexture(crt->gpu, crt->texture);
  SDL_ReleaseWindowFromGPUDevice(crt->gpu, window);
  SDL_DestroyGPUDevice(crt->gpu);
  free(crt);
}
