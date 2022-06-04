/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_reverse_uv_lookup.hh"

#include "BLI_bounds.hh"
#include "BLI_math_geom.h"

namespace blender::geometry {

ReverseUVLookup::ReverseUVLookup(const Span<float2> uv_map, const Span<MLoopTri> looptris)
    : uv_map_(uv_map), looptris_(looptris)
{
  grid_resolution_ = std::max<int>(std::sqrt(looptris.size()), 2);
  grid_.reinitialize(grid_resolution_ * grid_resolution_);

  if (looptris.size() == 0) {
    return;
  }
  bounds::MinMaxResult<float2> uv_bounds = *bounds::min_max(uv_map);
  grid_offset_ = uv_bounds.min;
  grid_scale_ = (uv_bounds.max - uv_bounds.min) * grid_resolution_ * 0.999f;

  for (const int looptri_index : looptris.index_range()) {
    const MLoopTri &looptri = looptris[looptri_index];
    const std::array<int, 3> corners = {static_cast<int>(looptri.tri[0]),
                                        static_cast<int>(looptri.tri[1]),
                                        static_cast<int>(looptri.tri[2])};
    const std::array<float2, 3> corner_uvs = {
        uv_map[corners[0]], uv_map[corners[1]], uv_map[corners[2]]};
    const std::array<int2, 3> corner_cell_coords = {this->uv_to_cell_coord(corner_uvs[0]),
                                                    this->uv_to_cell_coord(corner_uvs[1]),
                                                    this->uv_to_cell_coord(corner_uvs[2])};
    bounds::MinMaxResult<int2> cell_bounds = *bounds::min_max<int2>(corner_cell_coords);
    for (int y = cell_bounds.min.y; y <= cell_bounds.max.y; y++) {
      const int first_cell_index_in_row = y * grid_resolution_;
      int x = cell_bounds.min.x;
      while (x <= cell_bounds.max.x) {
        const int2 cell_coord{x, y};
        const int cell_index = first_cell_index_in_row + x;
        Cell &cell = grid_[cell_index];
        const bool is_fully_contained = this->tri_covers_cell(cell_coord, corner_uvs);
        switch (cell.status) {
          case CellStatus::EmptyOrPartial: {
            cell.looptris.append(looptri_index);
            if (is_fully_contained) {
              cell.status = CellStatus::Full;
            }
            x++;
            break;
          }
          case CellStatus::Full: {
            if (is_fully_contained) {
              cell.status = CellStatus::FullInvalid;
            }
            cell.looptris.clear_and_make_inline();
            x++;
            break;
          }
          case CellStatus::FullInvalid: {
            x++;
            break;
          }
        }
      }
    }
  }
}

ReverseUVLookup::Result ReverseUVLookup::lookup(const float2 &query_uv) const
{
  const int2 cell_coord = this->uv_to_cell_coord(query_uv);
  if (cell_coord.x < 0 || cell_coord.y < 0 || cell_coord.x >= grid_resolution_ ||
      cell_coord.y >= grid_resolution_) {
    return {};
  }
  const int cell_index = cell_coord.y * grid_resolution_ + cell_coord.x;
  const Cell &cell = grid_[cell_index];
  if (cell.status == CellStatus::FullInvalid) {
    return {};
  }
  Result result;
  for (const int looptri_index : cell.looptris) {
    const MLoopTri &looptri = looptris_[looptri_index];
    const float2 &uv0 = uv_map_[looptri.tri[0]];
    const float2 &uv1 = uv_map_[looptri.tri[1]];
    const float2 &uv2 = uv_map_[looptri.tri[2]];
    float3 bary_weights;
    if (!barycentric_coords_v2(uv0, uv1, uv2, query_uv, bary_weights)) {
      continue;
    }
    if (IN_RANGE_INCL(bary_weights.x, 0.0f, 1.0f) && IN_RANGE_INCL(bary_weights.y, 0.0f, 1.0f) &&
        IN_RANGE_INCL(bary_weights.z, 0.0f, 1.0f)) {
      if (result.type == ResultType::Ok) {
        result.type = ResultType::Multiple;
        result.looptri = nullptr;
        return result;
      }
      result = Result{ResultType::Ok, &looptri, bary_weights};
    }
  }
  return result;
}

int2 ReverseUVLookup::uv_to_cell_coord(const float2 &uv) const
{
  const float2 shifted = uv - grid_offset_;
  const float2 scaled = shifted * grid_scale_;
  return int2(scaled);
}

float2 ReverseUVLookup::cell_coord_to_uv(const int2 &cell_coord) const
{
  const float2 scaled = float2(cell_coord);
  const float2 shifted = scaled / grid_scale_;
  const float2 uv = shifted + grid_offset_;
  return uv;
}

bool ReverseUVLookup::tri_covers_cell(const int2 &cell_coord,
                                      const std::array<float2, 3> &tri) const
{
  for (const int x : IndexRange(2)) {
    for (const int y : IndexRange(2)) {
      if (!isect_point_tri_v2(
              this->cell_coord_to_uv(cell_coord + int2(x, y)), tri[0], tri[1], tri[2])) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace blender::geometry
