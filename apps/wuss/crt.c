/* wuss/crt.c -- CRT post-effect for the Wuss SDL frontend */

#ifdef WUSS_APP
#ifdef USE_SDL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include <SDL3/SDL.h>

#include "crt.h"

/* ----------------------------------------------------------------------- */

/* Shader knobs, pushed to the fragment shader as a uniform each frame. Must
 * match the MSL Params struct below field-for-field: plain floats, same
 * order, no padding. Chase H.Q.'s time-driven glitch knob is left out. */
typedef struct wuss_crt_params
{
  float curvature;          /* barrel distortion strength */
  float bloom_threshold;    /* level above which bloom kicks in */
  float bloom_intensity;    /* bloom contribution scale */
  float brightness;         /* post-contrast multiplicative brightness */
  float contrast;           /* contrast around mid-grey */
  float saturation;         /* 1.0 = full colour, 0.0 = greyscale */
  float scanline_intensity; /* base scanline darkening amount */
  float vignette_strength;  /* corner darkening strength */
  float chroma_bleed;       /* PAL horizontal colour smear, 0.0 = off */
}
wuss_crt_params_t;

struct wuss_crt
{
  SDL_GPUDevice           *gpu;
  SDL_GPUTexture          *texture;  /* holds the uploaded framebuffer */
  SDL_GPUTransferBuffer   *transfer; /* staging buffer for the upload */
  int                      width;    /* size texture/transfer were made at */
  int                      height;
  SDL_GPUSampler          *sampler;
  SDL_GPUGraphicsPipeline *pipeline;
};

/* ----------------------------------------------------------------------- */

/* ponytail: Chase H.Q.'s by-eye tuning, fixed; plumb through if wuss ever
 * wants a knob */
static const wuss_crt_params_t wuss_crt_params =
{
  0.025f, /* curvature */
  0.5f,   /* bloom_threshold */
  0.025f, /* bloom_intensity */
  1.1f,   /* brightness */
  1.1f,   /* contrast */
  1.0f,   /* saturation */
  0.75f,  /* scanline_intensity */
  0.3f,   /* vignette_strength */
  0.6f    /* chroma_bleed */
};

/* Fullscreen triangle generated from vertex_id, so no vertex buffer. */
static const char wuss_crt_vertex_msl[] =
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

/* Chase H.Q.'s shader with its Spectrum-specific 256x192 texel size and
 * 192-line scanline count taken from the texture instead, and alpha forced
 * opaque (bgrx's X byte is undefined). */
static const char wuss_crt_fragment_msl[] =
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
  "};\n"
  "fragment float4 fs_main(VSOut in [[stage_in]],\n"
  "                        texture2d<float> tex [[texture(0)]],\n"
  "                        sampler samp [[sampler(0)]],\n"
  "                        constant Params& p [[buffer(0)]]) {\n"
  /* barrel distortion via dot(coord,coord) radial distance */
  "  float2 coord = in.uv * 2.0 - 1.0;\n"
  "  coord *= 1.0 + dot(coord, coord) * p.curvature;\n"
  "  float2 uv = coord * 0.5 + 0.5;\n"
  /* soft edge: smoothstep border fade rather than a jagged hard cutoff */
  "  float2 edge = smoothstep(float2(0.0), float2(0.005), uv) *\n"
  "                smoothstep(float2(0.0), float2(0.005), 1.0 - uv);\n"
  "  float edgeMask = edge.x * edge.y;\n"
  "  float2 uvc = clamp(uv, 0.0, 1.0);\n"
  "  float2 size = float2(tex.get_width(), tex.get_height());\n"
  "  float2 texel = 1.0 / size;\n"
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
  "  c.rgb *= p.brightness;\n"
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

/* ----------------------------------------------------------------------- */

static SDL_GPUShader *wuss_crt_shader(SDL_GPUDevice     *gpu,
                                      const char        *code,
                                      const char        *entrypoint,
                                      SDL_GPUShaderStage stage,
                                      Uint32             nsamplers)
{
  SDL_GPUShaderCreateInfo info;

  memset(&info, 0, sizeof(info));
  info.code                = (const Uint8 *) code;
  info.code_size           = strlen(code);
  info.entrypoint          = entrypoint;
  info.format              = SDL_GPU_SHADERFORMAT_MSL;
  info.stage               = stage;
  info.num_samplers        = nsamplers;
  info.num_uniform_buffers = nsamplers; /* the fragment stage has one of each */

  return SDL_CreateGPUShader(gpu, &info);
}

result_t wuss_crt_create(SDL_Window *window, wuss_crt_t **pcrt)
{
  wuss_crt_t                       *crt;
  SDL_GPUSamplerCreateInfo          sampler_info;
  SDL_GPUShader                    *vertex_shader;
  SDL_GPUShader                    *fragment_shader;
  SDL_GPUColorTargetDescription     target;
  SDL_GPUGraphicsPipelineCreateInfo pipeline_info;

  crt = calloc(1, sizeof(*crt));
  if (crt == NULL)
    return result_OOM;

  crt->gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, NULL);
  if (crt->gpu == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateGPUDevice: %s\n", SDL_GetError());
    free(crt);
    return result_NOT_SUPPORTED;
  }

  if (!SDL_ClaimWindowForGPUDevice(crt->gpu, window))
  {
    fprintf(stderr, "Error: SDL_ClaimWindowForGPUDevice: %s\n", SDL_GetError());
    SDL_DestroyGPUDevice(crt->gpu);
    free(crt);
    return result_NOT_SUPPORTED;
  }

  /* linear filtering: the soft look is part of the effect */
  memset(&sampler_info, 0, sizeof(sampler_info));
  sampler_info.min_filter     = SDL_GPU_FILTER_LINEAR;
  sampler_info.mag_filter     = SDL_GPU_FILTER_LINEAR;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  crt->sampler = SDL_CreateGPUSampler(crt->gpu, &sampler_info);

  vertex_shader   = wuss_crt_shader(crt->gpu, wuss_crt_vertex_msl, "vs_main",
                                    SDL_GPU_SHADERSTAGE_VERTEX, 0);
  fragment_shader = wuss_crt_shader(crt->gpu, wuss_crt_fragment_msl, "fs_main",
                                    SDL_GPU_SHADERSTAGE_FRAGMENT, 1);

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
    wuss_crt_destroy(crt, window);
    return result_NOT_SUPPORTED;
  }

  *pcrt = crt;
  return result_OK;
}

/* (Re)make the texture and transfer buffer at width x height. */
static bool wuss_crt_size(wuss_crt_t *crt, int width, int height)
{
  SDL_GPUTextureCreateInfo        texture_info;
  SDL_GPUTransferBufferCreateInfo transfer_info;

  SDL_ReleaseGPUTexture(crt->gpu, crt->texture);
  SDL_ReleaseGPUTransferBuffer(crt->gpu, crt->transfer);
  crt->width  = 0;
  crt->height = 0;

  /* B8G8R8A8 matches bgrx8888's in-memory byte order */
  memset(&texture_info, 0, sizeof(texture_info));
  texture_info.type                 = SDL_GPU_TEXTURETYPE_2D;
  texture_info.format               = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
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

void wuss_crt_render(wuss_crt_t *crt,
                     SDL_Window *window,
                     const void *pixels,
                     int         width,
                     int         height)
{
  void                        *mapped;
  SDL_GPUTextureTransferInfo   src;
  SDL_GPUTextureRegion         dst;
  SDL_GPUCommandBuffer        *cmdbuf;
  SDL_GPUCopyPass             *copy_pass;
  SDL_GPUTexture              *swapchain;
  SDL_GPUColorTargetInfo       target;
  SDL_GPURenderPass           *render_pass;
  SDL_GPUTextureSamplerBinding binding;

  if ((width != crt->width || height != crt->height) &&
      !wuss_crt_size(crt, width, height))
    return;

  /* ponytail: uploads the whole frame each present, ignoring the dirty
   * rect; a partial upload would save bandwidth if it ever matters */
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
                                            NULL, NULL) &&
      swapchain != NULL)
  {
    memset(&target, 0, sizeof(target));
    target.texture       = swapchain;
    target.load_op       = SDL_GPU_LOADOP_CLEAR;
    target.store_op      = SDL_GPU_STOREOP_STORE;
    target.clear_color.a = 1.0f;

    render_pass = SDL_BeginGPURenderPass(cmdbuf, &target, 1, NULL);

    binding.texture = crt->texture;
    binding.sampler = crt->sampler;

    SDL_BindGPUGraphicsPipeline(render_pass, crt->pipeline);
    SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
    SDL_PushGPUFragmentUniformData(cmdbuf, 0, &wuss_crt_params,
                                   sizeof(wuss_crt_params));
    SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);

    SDL_EndGPURenderPass(render_pass);
  }

  SDL_SubmitGPUCommandBuffer(cmdbuf);
}

void wuss_crt_destroy(wuss_crt_t *crt, SDL_Window *window)
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

#endif /* USE_SDL */
#endif /* WUSS_APP */
