/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <mutex>
#include <variant>

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "spreadsheet_draw.hh"

namespace blender::ed::spreadsheet {

class CellValue {
 public:
  std::variant<std::monostate, int, float, bool, std::string> value_;

  void set_empty()
  {
    value_ = std::monostate();
  }

  void set_int(int value)
  {
    value_ = value;
  }

  void set_float(float value)
  {
    value_ = value;
  }

  void set_string(std::string value)
  {
    value_ = std::move(value);
  }
};

class ColumnDataProvider {
 private:
  std::string column_id_;
  std::string name_;

 public:
  ColumnDataProvider(std::string column_id, std::string name)
      : column_id_(std::move(column_id)), name_(std::move(name))
  {
  }

  virtual ~ColumnDataProvider() = default;

  virtual void get_value(int row_index, CellValue &value) const = 0;

  StringRefNull column_id() const
  {
    return column_id_;
  }

  StringRefNull name() const
  {
    return name_;
  }
};

class SpreadsheetDataProvider {
 private:
  mutable std::mutex cached_columns_mutex_;
  mutable Map<std::string, std::unique_ptr<ColumnDataProvider>> cached_columns_;

 public:
  virtual ~SpreadsheetDataProvider() = default;

  virtual void foreach_column_id(FunctionRef<void(StringRefNull)> UNUSED(callback)) const
  {
  }

  const ColumnDataProvider *try_get_column(StringRef column_id) const;

 protected:
  virtual std::unique_ptr<ColumnDataProvider> try_make_column_provider(
      StringRef column_id) const = 0;
};

struct SpreadsheetLayout {
  Vector<std::string> column_ids;
  Vector<int> column_widths_;
  Span<int64_t> row_indices;
};

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_data_provider(
    const SpreadsheetDataProvider &provider, const SpreadsheetLayout &layout);

}  // namespace blender::ed::spreadsheet
