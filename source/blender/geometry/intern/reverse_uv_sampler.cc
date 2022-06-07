/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_reverse_uv_sampler.hh"

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_math_geom.h"
#include "BLI_vector.hh"

namespace blender::geometry {

enum class CellType {
  NoneOrMultiple = 0,
  SingleFull = 1,
  Single = 2,
  Grid = 3,
};

class Cell {
 private:
  static constexpr uint64_t enum_size_in_bits = 2;
  static constexpr uint64_t enum_mask = (1 << enum_size_in_bits) - 1;

  uint64_t data_ = 0;

 public:
  Cell()
  {
    this->set_empty();
  }

  void set_empty()
  {
    data_ = 0;
  }

  void set_grid(ReverseUVSamplerGrid &grid)
  {
    data_ = reinterpret_cast<uint64_t>(&grid);
    BLI_assert((data_ & enum_mask) == 0);
    data_ |= static_cast<uint64_t>(CellType::Grid);
  }

  void set_single(const int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index <= INT32_MAX);
    data_ = index << 32;
    data_ |= static_cast<uint64_t>(CellType::Single);
  }

  void set_single_full(const int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index <= INT32_MAX);
    data_ = index << index;
    data_ |= static_cast<uint64_t>(CellType::SingleFull);
  }

  void set_multiple(const int64_t start, const int64_t amount)
  {
    BLI_assert(start >= 0);
    BLI_assert(amount >= 0);
    BLI_assert(start <= INT32_MAX);
    BLI_assert(amount <= INT16_MAX);
    data_ = (start << 32) | (amount << 16);
    data_ |= static_cast<uint64_t>(CellType::NoneOrMultiple);
  }

  CellType type() const
  {
    return static_cast<CellType>(data_ & enum_mask);
  }

  ReverseUVSamplerGrid &grid() const
  {
    BLI_assert(this->type() == CellType::Grid);
    return *reinterpret_cast<ReverseUVSamplerGrid *>(data_ & ~enum_mask);
  }

  int single() const
  {
    BLI_assert(this->type() == CellType::Single);
    return static_cast<int>(data_ >> 32);
  }

  int single_full() const
  {
    return this->single();
  }

  IndexRange none_or_multiple() const
  {
    BLI_assert(this->type() == CellType::NoneOrMultiple);
    const int64_t start = static_cast<int64_t>(data_ >> 32);
    const int64_t amount = static_cast<int64_t>(static_cast<uint32_t>(data_) >> 16);
    return IndexRange(start, amount);
  }
};

class ReverseUVSamplerGrid {
 private:
  int resolution_;
  float2 offset_;
  float2 scale_;
  Array<Cell, 0> cells_;

 public:
  ReverseUVSamplerGrid(const int resolution, const float2 offset, const float2 scale)
      : resolution_(resolution),
        offset_(offset),
        scale_(scale * resolution),
        cells_(resolution * resolution)
  {
  }

  const Cell &get(const int x, const int y) const
  {
    BLI_assert(x >= 0);
    BLI_assert(x < resolution_);
    BLI_assert(y >= 0);
    BLI_assert(y < resolution_);
    /* TODO: Can we shift by resolution_ instead? */
    const int cell_index = y * resolution_ + x;
    return cells_[cell_index];
  }

  Cell &get(const int x, const int y)
  {
    return const_cast<Cell &>(std::as_const(*this).get(x, y));
  }

  const Cell &get(const float2 &uv) const
  {
    const int2 cell_coords = this->get_cell_coords(uv);
    return this->get(cell_coords.x, cell_coords.y);
  }

  int2 get_cell_coords(const float2 &uv) const
  {
    const float2 uv_in_grid_space = (uv - offset_) * scale_;
    const int2 cell_coords{uv_in_grid_space};
    return cell_coords;
  }
};

ReverseUVSampler::ReverseUVSampler(const Span<float2> uv_map, const Span<MLoopTri> looptris)
    : uv_map_(uv_map), looptris_(looptris)
{
  if (uv_map.is_empty()) {
    return;
  }
  if (looptris_.is_empty()) {
    return;
  }

  const bounds::MinMaxResult<float2> uv_bounds = *bounds::min_max(uv_map);
  const int initial_resolution = std::sqrt(std::max<int64_t>(looptris_.size(), 4)) * 2;
  const float2 offset = uv_bounds.min;

  /* Increase slightly to make sure that uvs are definitely in the range. */
  const float2 scale = (uv_bounds.max - uv_bounds.min) * 1.001f;
  grid_ = std::make_unique<ReverseUVSamplerGrid>(initial_resolution, offset, scale);
  ReverseUVSamplerGrid &grid = *grid_;

  bool found_invalid_data = false;

  Vector<int> multi_cell_indices;

  for (const int looptri_index : looptris_.index_range()) {
    const MLoopTri &looptri = looptris_[looptri_index];

    const float2 &uv0 = uv_map_[looptri.tri[0]];
    const float2 &uv1 = uv_map_[looptri.tri[1]];
    const float2 &uv2 = uv_map_[looptri.tri[2]];

    const int2 cell_coords_0 = grid.get_cell_coords(uv0);
    const int2 cell_coords_1 = grid.get_cell_coords(uv1);
    const int2 cell_coords_2 = grid.get_cell_coords(uv2);

    const int2 min_cell_coord = math::min(math::min(cell_coords_0, cell_coords_1), cell_coords_2);
    const int2 max_cell_coord = math::max(math::max(cell_coords_0, cell_coords_1), cell_coords_2);

    bool current_looptri_is_invalid = false;

    for (int y = min_cell_coord.y; y <= max_cell_coord.y; y++) {
      for (int x = min_cell_coord.x; x <= max_cell_coord.x; x++) {
        Cell &cell = grid.get(x, y);
        const CellType old_cell_type = cell.type();
        switch (old_cell_type) {
          case CellType::NoneOrMultiple: {
            const IndexRange range = cell.none_or_multiple();
            if (range.is_empty()) {
              cell.set_single(looptri_index);
            }
            else {
              const int old_start = range.start();
              const int old_size = range.size();
              const int old_capacity = power_of_2_max_i(old_size);
              const int new_size = old_size + 1;

              if (old_capacity >= new_size) {
                multi_cell_indices[old_start + old_size] = looptri_index;
                cell.set_multiple(old_start, new_size);
              }
              else {
                const int new_start = multi_cell_indices.size();
                const int new_capacity = power_of_2_max_i(new_size);

                if (new_capacity <= 32) {
                  /* TODO: make new grid */
                }
                else {
                  multi_cell_indices.resize(new_start + new_capacity);
                  uninitialized_copy_n(multi_cell_indices.data() + old_start,
                                       old_size,
                                       multi_cell_indices.data() + new_start);
                  multi_cell_indices[new_start + old_size] = looptri_index;
                  cell.set_multiple(new_start, new_size);
                }
              }
            }
            break;
          }
          case CellType::SingleFull: {
            current_looptri_is_invalid = true;
            break;
          }
          case CellType::Single: {
            const int other_looptri_index = cell.single();
            multi_cell_indices.append(other_looptri_index);
            multi_cell_indices.append(looptri_index);
            cell.set_multiple(multi_cell_indices.size() - 2, 2);
            break;
          }
          case CellType::Grid: {
            /* TODO: insert in grid */
            break;
          }
        }
      }

      if (current_looptri_is_invalid) {
        break;
      }
    }
  }
}

ReverseUVSampler::Result ReverseUVSampler::sample(const float2 &query_uv) const
{
  for (const MLoopTri &looptri : looptris_) {
    const float2 &uv0 = uv_map_[looptri.tri[0]];
    const float2 &uv1 = uv_map_[looptri.tri[1]];
    const float2 &uv2 = uv_map_[looptri.tri[2]];
    float3 bary_weights;
    if (!barycentric_coords_v2(uv0, uv1, uv2, query_uv, bary_weights)) {
      continue;
    }
    if (IN_RANGE_INCL(bary_weights.x, 0.0f, 1.0f) && IN_RANGE_INCL(bary_weights.y, 0.0f, 1.0f) &&
        IN_RANGE_INCL(bary_weights.z, 0.0f, 1.0f)) {
      return Result{ResultType::Ok, &looptri, bary_weights};
    }
  }
  return Result{};
}

}  // namespace blender::geometry
