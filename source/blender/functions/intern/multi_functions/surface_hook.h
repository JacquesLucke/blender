#pragma once

#include "FN_multi_function.h"

namespace FN {

class MF_ClosestSurfaceHookOnObject final : public MultiFunction {
 public:
  MF_ClosestSurfaceHookOnObject();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_GetPositionOnSurface final : public MultiFunction {
 public:
  MF_GetPositionOnSurface();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_GetNormalOnSurface final : public MultiFunction {
 public:
  MF_GetNormalOnSurface();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_GetWeightOnSurface final : public MultiFunction {
 private:
  std::string m_vertex_group_name;

 public:
  MF_GetWeightOnSurface(std::string vertex_group_name);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_GetImageColorOnSurface final : public MultiFunction {
 public:
  MF_GetImageColorOnSurface();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

}  // namespace FN
