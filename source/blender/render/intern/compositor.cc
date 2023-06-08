/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_node.hh"
#include "BKE_scene.h"

#include "DRW_engine.h"

#include "COM_context.hh"
#include "COM_evaluator.hh"

#include "RE_compositor.hh"
#include "RE_pipeline.h"

namespace blender::render {

/* Render Texture Pool */

class TexturePool : public realtime_compositor::TexturePool {
 public:
  Vector<GPUTexture *> textures_;

  virtual ~TexturePool()
  {
    for (GPUTexture *texture : textures_) {
      GPU_texture_free(texture);
    }
  }

  GPUTexture *allocate_texture(int2 size, eGPUTextureFormat format) override
  {
    /* TODO: should share pool with draw manager. It needs some globals
     * initialization figured out there first. */
#if 0
    DrawEngineType *owner = (DrawEngineType *)this;
    return DRW_texture_pool_query_2d(size.x, size.y, format, owner);
#else
    GPUTexture *texture = GPU_texture_create_2d(
        "compositor_texture_pool", size.x, size.y, 1, format, GPU_TEXTURE_USAGE_GENERAL, NULL);
    textures_.append(texture);
    return texture;
#endif
  }
};

/* Render Context */

class Context : public realtime_compositor::Context {
 private:
  /* Input data. */
  const Scene &scene_;
  const RenderData &render_data_;
  const bNodeTree &node_tree_;
  const bool use_file_output_;
  const char *view_name_;

  /* Output combined texture. */
  GPUTexture *output_texture_ = nullptr;

 public:
  Context(const Scene &scene,
          const RenderData &render_data,
          const bNodeTree &node_tree,
          const bool use_file_output,
          const char *view_name,
          TexturePool &texture_pool)
      : realtime_compositor::Context(texture_pool),
        scene_(scene),
        render_data_(render_data),
        node_tree_(node_tree),
        use_file_output_(use_file_output),
        view_name_(view_name)
  {
  }

  virtual ~Context()
  {
    GPU_texture_free(output_texture_);
  }

  const bNodeTree &get_node_tree() const override
  {
    return node_tree_;
  }

  bool use_file_output() const override
  {
    return use_file_output_;
  }

  bool use_texture_color_management() const override
  {
    return BKE_scene_check_color_management_enabled(&scene_);
  }

  const RenderData &get_render_data() const override
  {
    return render_data_;
  }

  int2 get_render_size() const override
  {
    int width, height;
    BKE_render_resolution(&render_data_, false, &width, &height);
    return int2(width, height);
  }

  rcti get_compositing_region() const override
  {
    const int2 render_size = get_render_size();
    const rcti render_region = rcti{0, render_size.x, 0, render_size.y};

    return render_region;
  }

  GPUTexture *get_output_texture() override
  {
    /* TODO: support outputting for viewers and previews.
     * TODO: just a temporary hack, needs to get stored in RenderResult,
     * once that supports GPU buffers. */
    if (output_texture_ == nullptr) {
      const int2 size = get_render_size();
      output_texture_ = GPU_texture_create_2d("compositor_output_texture",
                                              size.x,
                                              size.y,
                                              1,
                                              GPU_RGBA16F,
                                              GPU_TEXTURE_USAGE_GENERAL,
                                              NULL);
    }

    return output_texture_;
  }

  GPUTexture *get_input_texture(int view_layer_id, const char *pass_name) override
  {
    /* TODO: eventually this should get cached on the RenderResult itself when
     * it supports storing GPU buffers, for faster updates. But will also need
     * some eviction strategy to avoid too much GPU memory usage. */
    Render *re = RE_GetSceneRender(&scene_);
    RenderResult *rr = nullptr;
    GPUTexture *input_texture = nullptr;

    if (re) {
      rr = RE_AcquireResultRead(re);
    }

    if (rr) {
      ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene_.view_layers, view_layer_id);
      if (view_layer) {
        RenderLayer *rl = RE_GetRenderLayer(rr, view_layer->name);
        if (rl) {
          RenderPass *rpass = (RenderPass *)BLI_findstring(
              &rl->passes, pass_name, offsetof(RenderPass, name));

          if (rpass && rpass->buffer.data) {
            const int2 size(rl->rectx, rl->recty);

            if (rpass->channels == 1) {
              input_texture = texture_pool().acquire_float(size);
              if (input_texture) {
                GPU_texture_update(input_texture, GPU_DATA_FLOAT, rpass->buffer.data);
              }
            }
            else if (rpass->channels == 3) {
              input_texture = texture_pool().acquire_color(size);
              if (input_texture) {
                /* TODO: conversion could be done as part of GPU upload somehow? */
                const float *rgb_buffer = rpass->buffer.data;
                Vector<float> rgba_buffer(4 * size.x * size.y);
                for (size_t i = 0; i < (size_t)size.x * (size_t)size.y; i++) {
                  rgba_buffer[i * 4 + 0] = rgb_buffer[i * 3 + 0];
                  rgba_buffer[i * 4 + 1] = rgb_buffer[i * 3 + 1];
                  rgba_buffer[i * 4 + 2] = rgb_buffer[i * 3 + 2];
                  rgba_buffer[i * 4 + 3] = 1.0f;
                }
                GPU_texture_update(input_texture, GPU_DATA_FLOAT, rgba_buffer.data());
              }
            }
            else if (rpass->channels == 4) {
              input_texture = texture_pool().acquire_color(size);
              if (input_texture) {
                GPU_texture_update(input_texture, GPU_DATA_FLOAT, rpass->buffer.data);
              }
            }
          }
        }
      }
    }

    if (re) {
      RE_ReleaseResult(re);
      re = nullptr;
    }

    return input_texture;
  }

  StringRef get_view_name() override
  {
    return view_name_;
  }

  void set_info_message(StringRef /* message */) const override
  {
    /* TODO: ignored for now. Currently only used to communicate incomplete node support
     * which is already shown on the node itself.
     *
     * Perhaps this overall info message could be replaced by a boolean indicating
     * incomplete support, and leave more specific message to individual nodes? */
  }

  IDRecalcFlag query_id_recalc_flag(ID * /* id */) const override
  {
    /* TODO: implement? */
    return IDRecalcFlag(0);
  }

  void output_to_render_result()
  {
    Render *re = RE_GetSceneRender(&scene_);
    RenderResult *rr = RE_AcquireResultWrite(re);

    if (rr) {
      RenderView *rv = RE_RenderViewGetByName(rr, view_name_);

      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      float *output_buffer = (float *)GPU_texture_read(output_texture_, GPU_DATA_FLOAT, 0);

      if (output_buffer) {
        RE_RenderBuffer_assign_data(&rv->combined_buffer, output_buffer);
      }

      /* TODO: z-buffer output. */

      rr->have_combined = true;
    }

    if (re) {
      RE_ReleaseResult(re);
      re = nullptr;
    }

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_R_RESULT, "Render Result");
    BKE_image_partial_update_mark_full_update(image);
    BLI_thread_lock(LOCK_DRAW_IMAGE);
    BKE_image_signal(G.main, image, nullptr, IMA_SIGNAL_FREE);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }
};

/* Render Realtime Compositor */

RealtimeCompositor::RealtimeCompositor(Render &render,
                                       const Scene &scene,
                                       const RenderData &render_data,
                                       const bNodeTree &node_tree,
                                       const bool use_file_output,
                                       const char *view_name)
    : render_(render)
{
  /* Create resources with GPU context enabled. */
  DRW_render_context_enable(&render_);
  texture_pool_ = std::make_unique<TexturePool>();
  context_ = std::make_unique<Context>(
      scene, render_data, node_tree, use_file_output, view_name, *texture_pool_);
  evaluator_ = std::make_unique<realtime_compositor::Evaluator>(*context_);
  DRW_render_context_disable(&render_);
}

RealtimeCompositor::~RealtimeCompositor()
{
  /* Free resources with GPU context enabled. */
  DRW_render_context_enable(&render_);
  evaluator_.reset();
  context_.reset();
  texture_pool_.reset();
  DRW_render_context_disable(&render_);
}

void RealtimeCompositor::execute()
{
  DRW_render_context_enable(&render_);
  evaluator_->evaluate();
  context_->output_to_render_result();
  DRW_render_context_disable(&render_);
}

void RealtimeCompositor::update(const Depsgraph * /* depsgraph */)
{
  /* TODO: implement */
}

}  // namespace blender::render