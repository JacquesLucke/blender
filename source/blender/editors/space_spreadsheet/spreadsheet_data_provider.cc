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

#include <iomanip>
#include <sstream>

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLF_api.h"

#include "spreadsheet_data_provider.hh"

namespace blender::ed::spreadsheet {

const ColumnDataProvider *SpreadsheetDataProvider::try_get_column(StringRef column_id) const
{
  std::lock_guard lock(cached_columns_mutex_);
  return cached_columns_
      .lookup_or_add_cb_as(column_id, [&]() { return this->try_make_column_provider(column_id); })
      .get();
}

class SpreadsheetDrawerForDataProvider : public SpreadsheetDrawer {
 private:
  const SpreadsheetDataProvider &provider_;
  const SpreadsheetLayout &layout_;
  Vector<const ColumnDataProvider *> columns_;

 public:
  SpreadsheetDrawerForDataProvider(const SpreadsheetDataProvider &provider,
                                   const SpreadsheetLayout &layout)
      : provider_(provider), layout_(layout)
  {
    for (StringRef column_id : layout.column_ids) {
      columns_.append(provider.try_get_column(column_id));
    }

    this->tot_rows = layout.row_indices.size();
    this->tot_columns = columns_.size();
  }

  void draw_top_row_cell(int column_index, const CellDrawParams &params) const override
  {
    const ColumnDataProvider *column = columns_[column_index];
    if (column == nullptr) {
      return;
    }
    const StringRefNull name = column->name();
    uiBut *but = uiDefIconTextBut(params.block,
                                  UI_BTYPE_LABEL,
                                  0,
                                  ICON_NONE,
                                  name.c_str(),
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr);
    /* Center-align column headers. */
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
  }

  void draw_left_column_cell(int row_index, const CellDrawParams &params) const override
  {
    const int real_index = layout_.row_indices[row_index];
    std::string index_str = std::to_string(real_index);
    uiBut *but = uiDefIconTextBut(params.block,
                                  UI_BTYPE_LABEL,
                                  0,
                                  ICON_NONE,
                                  index_str.c_str(),
                                  params.xmin,
                                  params.ymin,
                                  params.width,
                                  params.height,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  0,
                                  nullptr);
    /* Right-align indices. */
    UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);
    UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  }

  void draw_content_cell(int row_index,
                         int column_index,
                         const CellDrawParams &params) const override
  {
    const ColumnDataProvider *column = columns_[column_index];
    if (column == nullptr) {
      return;
    }
    const int real_index = layout_.row_indices[row_index];
    CellValue cell_value;
    column->get_value(real_index, cell_value);

    if (std::holds_alternative<std::monostate>(cell_value.value_)) {
      /* Cell is empty. */
    }
    else if (std::holds_alternative<int>(cell_value.value_)) {
      const int value = std::get<int>(cell_value.value_);
      const std::string value_str = std::to_string(value);
      uiDefIconTextBut(params.block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_NONE,
                       value_str.c_str(),
                       params.xmin,
                       params.ymin,
                       params.width,
                       params.height,
                       nullptr,
                       0,
                       0,
                       0,
                       0,
                       nullptr);
    }
    else if (std::holds_alternative<float>(cell_value.value_)) {
      const float value = std::get<float>(cell_value.value_);
      std::stringstream ss;
      ss << std::fixed << std::setprecision(3) << value;
      const std::string value_str = ss.str();
      uiDefIconTextBut(params.block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_NONE,
                       value_str.c_str(),
                       params.xmin,
                       params.ymin,
                       params.width,
                       params.height,
                       nullptr,
                       0,
                       0,
                       0,
                       0,
                       nullptr);
    }
    else if (std::holds_alternative<bool>(cell_value.value_)) {
      const bool value = std::get<bool>(cell_value.value_);
      const int icon = value ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT;
      uiDefIconTextBut(params.block,
                       UI_BTYPE_LABEL,
                       0,
                       icon,
                       "",
                       params.xmin,
                       params.ymin,
                       params.width,
                       params.height,
                       nullptr,
                       0,
                       0,
                       0,
                       0,
                       nullptr);
    }
    else if (std::holds_alternative<std::string>(cell_value.value_)) {
      const std::string &value = std::get<std::string>(cell_value.value_);
      uiDefIconTextBut(params.block,
                       UI_BTYPE_LABEL,
                       0,
                       ICON_NONE,
                       value.c_str(),
                       params.xmin,
                       params.ymin,
                       params.width,
                       params.height,
                       nullptr,
                       0,
                       0,
                       0,
                       0,
                       nullptr);
    }
  }

  int column_width(int column_index) const override
  {
    return layout_.column_widths_[column_index];
  }
};

std::unique_ptr<SpreadsheetDrawer> spreadsheet_drawer_from_data_provider(
    const SpreadsheetDataProvider &provider, const SpreadsheetLayout &layout)
{
  return std::make_unique<SpreadsheetDrawerForDataProvider>(provider, layout);
}

}  // namespace blender::ed::spreadsheet
