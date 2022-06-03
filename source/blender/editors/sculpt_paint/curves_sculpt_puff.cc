/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

class PuffOperation : public CurvesSculptStrokeOperation {
 private:
  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  friend struct PuffOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct PuffOperationExecutor {
  PuffOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  PuffOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(PuffOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(C, stroke_extension);
    self_ = &self;
  }
};

void PuffOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  PuffOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_puff_operation()
{
  return std::make_unique<PuffOperation>();
}

}  // namespace blender::ed::sculpt_paint
