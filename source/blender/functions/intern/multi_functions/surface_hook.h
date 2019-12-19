#pragma once

#include "FN_multi_function.h"

namespace FN {

class MF_ClosestSurfaceHookOnObject final : public MultiFunction {
 public:
  MF_ClosestSurfaceHookOnObject();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_GetPositionOnSurface final : public MultiFunction {
 public:
  MF_GetPositionOnSurface();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_GetNormalOnSurface final : public MultiFunction {
 public:
  MF_GetNormalOnSurface();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_GetWeightOnSurface final : public MultiFunction {
 public:
  MF_GetWeightOnSurface();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_GetImageColorOnSurface final : public MultiFunction {
 public:
  MF_GetImageColorOnSurface();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_SampleObjectSurface final : public MultiFunction {
 private:
  bool m_use_vertex_weights;

 public:
  MF_SampleObjectSurface(bool use_vertex_weights);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

}  // namespace FN
