#include <stdio.h>

#include "drm-uapi/drm_fourcc.h"

#include "util/u_memory.h"
#include "util/u_screen.h"

#include "grate_common.h"
#include "grate_context.h"
#include "grate_resource.h"
#include "grate_screen.h"

#include "opentegra_lib.h"

static const struct debug_named_value debug_options[] = {
   { "unimplemented", GRATE_DEBUG_UNIMPLEMENTED,
     "Print unimplemented functions" },
   { "tgsi", GRATE_DEBUG_TGSI,
     "Dump TGSI during program compile" },
   { NULL }
};

DEBUG_GET_ONCE_FLAGS_OPTION(grate_debug, "GRATE_DEBUG", debug_options, 0)
uint32_t grate_debug;

static void
grate_screen_destroy(struct pipe_screen *pscreen)
{
   struct grate_screen *screen = grate_screen(pscreen);

   slab_destroy_parent(&screen->transfer_pool);

   drm_tegra_close(screen->drm);
   FREE(screen);
}

static const char *
grate_screen_get_name(struct pipe_screen *pscreen)
{
   return "Tegra";
}

static const char *
grate_screen_get_vendor(struct pipe_screen *pscreen)
{
   return "Grate";
}

static const char *
grate_screen_get_device_vendor(struct pipe_screen *pscreen)
{
   return "NVIDIA";
}

static int
grate_screen_get_screen_fd(struct pipe_screen *pscreen)
{
   return grate_screen(pscreen)->fd;
}

static void
grate_screen_init_caps(struct grate_screen *screen)
{
   struct pipe_caps *caps = (struct pipe_caps *)&screen->base.caps;

   u_init_pipe_screen_caps(&screen->base, 1);

   caps->npot_textures = true; /* not really, but mesa requires it for now! */

   caps->max_render_targets = 8; /* ??? */

   caps->max_texture_2d_size = 2048;

   caps->max_texture_3d_levels = 0;

   caps->max_texture_cube_levels = 16; /* ??? */

   caps->supported_prim_modes_with_restart = 0;

   caps->supported_prim_modes =
      BITFIELD_BIT(MESA_PRIM_POINTS) |
      BITFIELD_BIT(MESA_PRIM_LINES) |
      BITFIELD_BIT(MESA_PRIM_LINE_LOOP) |
      BITFIELD_BIT(MESA_PRIM_LINE_STRIP) |
      BITFIELD_BIT(MESA_PRIM_TRIANGLES) |
      BITFIELD_BIT(MESA_PRIM_TRIANGLE_STRIP) |
      BITFIELD_BIT(MESA_PRIM_TRIANGLE_FAN);

   caps->blend_equation_separate = true;

   /* well, not quite. but perhaps close enough? */
   caps->fragment_shader_texture_lod = true;
   caps->fragment_shader_derivatives = true;

   caps->min_texel_offset = caps->max_texel_offset = 0;

   caps->vertex_color_unclamped = true; /* probably irrelevant for GLES2 */

   caps->vertex_color_clamped = false; /* probably irrelevant for GLES2 */

   caps->glsl_feature_level = 120; /* no clue */

   caps->constant_buffer_offset_alignment = 4; /* DWORD aligned, can do pure data GATHER */

   caps->texture_transfer_modes = PIPE_TEXTURE_TRANSFER_BLIT;

   caps->mixed_framebuffer_sizes = true;

   caps->buffer_map_persistent_coherent = false; /* dunno */

   caps->vendor_id = 0x10de;

   caps->device_id = 0xFFFFFFFF;

   caps->accelerated = 1;

   caps->video_memory = 0;

   caps->uma = 1;

   caps->max_vertex_attrib_stride = (1 << 24) - 1;

   caps->mixed_color_depth_bits = 1; /* probably true ? */

   caps->fbfetch = false; /* TODO: supported, but let's enable later */
   caps->can_bind_const_buffer_as_vertex = false; /* TODO: probably */
   caps->allow_mapped_buffers_during_execution = false; /* TODO: probably */

   caps->max_varyings = 16;


   caps->min_line_width = caps->min_line_width_aa = 1.0f; /* no clue */
   caps->min_point_size = caps->min_point_size_aa = 1.0f; /* no clue */

   caps->max_line_width = caps->max_line_width_aa = 8192.0f; /* no clue */
   caps->max_point_size = caps->max_point_size_aa = 8192.0f; /* no clue */

   caps->line_width_granularity =
   caps->point_size_granularity = 1.0 / 16; /* not a real limit, HW uses floats... but helps caching CSOs */

   caps->max_texture_anisotropy = 0.0f;

   caps->max_texture_lod_bias = 16.0f;

   caps->min_conservative_raster_dilate =
   caps->max_conservative_raster_dilate =
   caps->conservative_raster_dilate_granularity = 0.0f;
}

static int
grate_screen_get_shader_param(struct pipe_screen *pscreen,
                              enum pipe_shader_type shader,
                              enum pipe_shader_cap param)
{
   switch (shader) {
   case PIPE_SHADER_VERTEX:
      switch (param) {

      case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
      case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
         return 1024;

      /* no vertex-texturing */
      case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
      case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
      case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
      case PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS:
         return 0;

      case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
      case PIPE_SHADER_CAP_CONT_SUPPORTED:
      case PIPE_SHADER_CAP_SUBROUTINES:
         return 0;

      case PIPE_SHADER_CAP_MAX_INPUTS:
      case PIPE_SHADER_CAP_MAX_OUTPUTS:
         return 16;

      case PIPE_SHADER_CAP_MAX_CONST_BUFFER0_SIZE:
         return 1024;

      case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
         return 1;

      case PIPE_SHADER_CAP_MAX_TEMPS:
         return 64 * 4; /* 64 vec4s */

      /* cannot index attributes, varyings nor GPRs */
      case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
         return 0;

      case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
         return 1; /* can index constant registers */

      case PIPE_SHADER_CAP_INTEGERS:
      case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
         return 0;

      case PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED:
         return 1;

      case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
         return 0;

      case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
         return 0;

      case PIPE_SHADER_CAP_SUPPORTED_IRS:
         return (1 << PIPE_SHADER_IR_TGSI) | (1 << PIPE_SHADER_IR_NIR);

      case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTERS:
      case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTER_BUFFERS:
          return 0;

      case PIPE_SHADER_CAP_FP16:
      case PIPE_SHADER_CAP_FP16_DERIVATIVES:
      case PIPE_SHADER_CAP_FP16_CONST_BUFFERS:
      case PIPE_SHADER_CAP_INT16:
      case PIPE_SHADER_CAP_GLSL_16BIT_CONSTS:
          return 0;

      default:
         fprintf(stdout, "%s: unsupported vertex-shader parameter: %d\n", __func__, param);
         return 0;
      }

   case PIPE_SHADER_FRAGMENT:

      switch (param) {
      case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
         return 4 * 128;

      case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
         return 4 * 128;

      case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
         return 128;

      case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
         return 128;

      /* no control flow */
      case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
      case PIPE_SHADER_CAP_CONT_SUPPORTED:
      case PIPE_SHADER_CAP_SUBROUTINES:
         return 0;

      case PIPE_SHADER_CAP_MAX_INPUTS:
      case PIPE_SHADER_CAP_MAX_OUTPUTS:
         return 16;

      case PIPE_SHADER_CAP_MAX_CONST_BUFFER0_SIZE:
         return 32;

      case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
         return 1;

      case PIPE_SHADER_CAP_MAX_TEMPS:
         return 16; /* scalars */

      /* no indirection */
      case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
      case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
         return 0;

      case PIPE_SHADER_CAP_INTEGERS:
      case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
         return 0;

      case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
      case PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS:
         return 16;

      case PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED:
         return 1;

      case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
         return 0;

      case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
         return 0;

      case PIPE_SHADER_CAP_SUPPORTED_IRS:
         return (1 << PIPE_SHADER_IR_TGSI) | (1 << PIPE_SHADER_IR_NIR);

      case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTERS:
      case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTER_BUFFERS:
          return 0;

      case PIPE_SHADER_CAP_FP16:
      case PIPE_SHADER_CAP_FP16_DERIVATIVES:
      case PIPE_SHADER_CAP_FP16_CONST_BUFFERS:
      case PIPE_SHADER_CAP_INT16:
      case PIPE_SHADER_CAP_GLSL_16BIT_CONSTS:
          return 0;

      default:
         fprintf(stdout, "%s: unsupported fragment-shader parameter: %d\n", __func__, param);
         return 0;
      }
      break;

   case PIPE_SHADER_GEOMETRY:
   case PIPE_SHADER_TESS_CTRL:
   case PIPE_SHADER_TESS_EVAL:
   case PIPE_SHADER_COMPUTE:
      return 0;

   default:
      fprintf(stdout, "%s: unknown shader type: %u\n", __func__, shader);
      return 0;
   }
}


static bool
grate_screen_is_format_supported(struct pipe_screen *pscreen,
                                 enum pipe_format format,
                                 enum pipe_texture_target target,
                                 unsigned sample_count,
                                 unsigned storage_sample_count,
                                 unsigned usage)
{
   if (usage & (PIPE_BIND_RENDER_TARGET | PIPE_BIND_DEPTH_STENCIL)) {
      if (grate_pixel_format(format) < 0)
         return false;
   }

   return true;
}

static void
grate_screen_fence_reference(struct pipe_screen *pscreen,
                             struct pipe_fence_handle **ptr,
                             struct pipe_fence_handle *fence)
{
   unimplemented();
}

static bool
grate_screen_fence_finish(struct pipe_screen *screen,
                          struct pipe_context *ctx,
                          struct pipe_fence_handle *fence,
                          uint64_t timeout)
{
   unimplemented();
   return false;
}

static const uint64_t grate_available_modifiers[] = {
   DRM_FORMAT_MOD_LINEAR,
};

static void
grate_screen_query_dmabuf_modifiers(struct pipe_screen *pscreen,
                                    enum pipe_format format, int max,
                                    uint64_t *modifiers,
                                    unsigned int *external_only,
                                    int *count)
{
   int num_modifiers = ARRAY_SIZE(grate_available_modifiers);

   if (!modifiers) {
      *count = num_modifiers;
      return;
   }

   *count = MIN2(max, num_modifiers);
   for (int i = 0; i < *count; i++) {
      modifiers[i] = grate_available_modifiers[i];
      if (external_only)
         external_only[i] = false;
   }
}

static bool
grate_screen_is_dmabuf_modifier_supported(struct pipe_screen *pscreen,
                                          uint64_t modifier,
                                          enum pipe_format format,
                                          bool *external_only)
{
   for (int i = 0; i < ARRAY_SIZE(grate_available_modifiers); i++) {
      if (grate_available_modifiers[i] == modifier) {
         if (external_only)
            *external_only = false;

         return true;
      }
   }

   return false;
}

struct pipe_screen *
grate_screen_create(int fd)
{
   struct drm_tegra_channel *drm_channel;
   struct grate_screen *screen;
   int err;

   screen = CALLOC_STRUCT(grate_screen);
   if (!screen)
      return NULL;

   screen->fd = fd;
   err = drm_tegra_new(&screen->drm, fd);
   if (err) {
      FREE(screen);
      return NULL;
   }

   err = drm_tegra_channel_open(&drm_channel, screen->drm, DRM_TEGRA_GR3D);
   if (err) {
      drm_tegra_close(screen->drm);
      FREE(screen);
      return NULL;
   }

   drm_tegra_channel_close(drm_channel);

   grate_debug = debug_get_option_grate_debug();

   screen->base.destroy = grate_screen_destroy;
   screen->base.get_name = grate_screen_get_name;
   screen->base.get_vendor = grate_screen_get_vendor;
   screen->base.get_device_vendor = grate_screen_get_device_vendor;
   screen->base.get_screen_fd = grate_screen_get_screen_fd;
   screen->base.get_shader_param = grate_screen_get_shader_param;
   screen->base.context_create = grate_screen_context_create;
   screen->base.is_format_supported = grate_screen_is_format_supported;
   screen->base.query_dmabuf_modifiers = grate_screen_query_dmabuf_modifiers;
   screen->base.is_dmabuf_modifier_supported = grate_screen_is_dmabuf_modifier_supported;

   /* fence functions */
   screen->base.fence_reference = grate_screen_fence_reference;
   screen->base.fence_finish = grate_screen_fence_finish;

   grate_screen_resource_init(&screen->base);

   slab_create_parent(&screen->transfer_pool, sizeof(struct pipe_transfer), 16);

   grate_screen_init_caps(screen);

   return &screen->base;
}
