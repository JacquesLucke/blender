/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LAYER_NAME_CT 4 /* u0123456789, u, au, a0123456789 */
#define MAX_LAYER_NAME_LEN (GPU_MAX_SAFE_ATTR_NAME + 2)
#define MAX_THICKRES 2    /* see eHairType */
#define MAX_HAIR_SUBDIV 4 /* see hair_subdiv rna */

typedef enum ParticleRefineShader {
  PART_REFINE_CATMULL_ROM = 0,
  PART_REFINE_MAX_SHADER,
} ParticleRefineShader;

struct ModifierData;
struct Object;
struct ParticleHairCache;
struct ParticleSystem;

typedef struct ParticleHairCache {
  GPUVertBuf *pos;
  GPUIndexBuf *indices;
  GPUBatch *hairs;

  /* Hair Procedural display: Interpolation is done on the GPU. */
  GPUVertBuf *proc_point_buf; /* Input control points */
  GPUTexture *point_tex;

  /** Infos of control points strands (segment count and base index) */
  GPUVertBuf *proc_strand_buf;
  GPUTexture *strand_tex;

  int num_uv_layers;
  int num_col_layers;

  int strands_len;
  int elems_len;
  int point_len;
} ParticleHairCache;

#ifdef __cplusplus
}
#endif
