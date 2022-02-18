/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

struct Object;

namespace blender::bke {

class CurvesSculptStroke {
 public:
  Vector<float2> mouse_positions;
  Vector<float3> ray_starts;
  Vector<float3> ray_ends;
};

class CurvesSculptSession {
 public:
  std::optional<CurvesSculptStroke> current_stroke;
};

CurvesSculptSession &curves_sculpt_session_ensure(struct Object &object);

}  // namespace blender::bke
