/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_meshdata_types.h"

namespace blender::geometry {

class ReverseUVLookup {
 private:
  const Span<float2> uv_map_;
  const Span<MLoopTri> looptris_;

  /** This is just an initial implementation to avoid quadratic run-time behavior. */
  enum class CellStatus {
    EmptyOrPartial,
    Full,
    FullInvalid,
  };
  struct Cell {
    CellStatus status = CellStatus::EmptyOrPartial;
    Vector<int, 2> looptris;
  };
  int grid_resolution_ = 0;
  Array<Cell> grid_;
  float2 grid_offset_;
  float2 grid_scale_;

 public:
  ReverseUVLookup(const Span<float2> uv_map, const Span<MLoopTri> looptris);

  enum class ResultType {
    None,
    Ok,
    Multiple,
  };

  struct Result {
    ResultType type = ResultType::None;
    const MLoopTri *looptri = nullptr;
    float3 bary_weights;
  };

  Result lookup(const float2 &query_uv) const;

 private:
  int2 uv_to_cell_coord(const float2 &uv) const;
  float2 cell_coord_to_uv(const int2 &cell_coord) const;
  bool tri_covers_cell(const int2 &cell_coord, const std::array<float2, 3> &tri) const;
};

}  // namespace blender::geometry
