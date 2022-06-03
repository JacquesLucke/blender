/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "curves_sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

class DensityOperation : public CurvesSculptStrokeOperation {
 private:
  /** Only used when a 3D brush is used. */
  CurvesBrush3D brush_3d_;

  friend struct DensityOperationExecutor;

 public:
  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct DensityOperationExecutor {
  DensityOperation *self_ = nullptr;
  CurvesSculptCommonContext ctx_;

  DensityOperationExecutor(const bContext &C) : ctx_(C)
  {
  }

  void execute(DensityOperation &self, const bContext &C, const StrokeExtension &stroke_extension)
  {
    UNUSED_VARS(C, stroke_extension);
    self_ = &self;
  }
};

void DensityOperation::on_stroke_extended(const bContext &C,
                                          const StrokeExtension &stroke_extension)
{
  DensityOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

std::unique_ptr<CurvesSculptStrokeOperation> new_density_operation()
{
  return std::make_unique<DensityOperation>();
}

}  // namespace blender::ed::sculpt_paint
