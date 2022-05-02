/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

class PuffOperation : public CurvesSculptStrokeOperation {
 private:
  float2 brush_pos_prev_re_;

  CurvesBrush3D brush_3d_;

  friend struct PuffOperationExecutor;

 public:
  void on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension) override;
};

struct PuffOperationExecutor {
  void execute(PuffOperation &self, bContext *C, const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(self, C, stroke_extension);
  }
};

void PuffOperation::on_stroke_extended(bContext *C, const StrokeExtension &stroke_extension)
{
  PuffOperationExecutor executor;
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_puff_operation()
{
  return std::make_unique<PuffOperation>();
}

}  // namespace blender::ed::sculpt_paint
