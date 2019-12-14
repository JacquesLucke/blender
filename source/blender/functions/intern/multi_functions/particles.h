#pragma once

#include "FN_multi_function.h"

namespace FN {

class MF_ParticleAttribute final : public MultiFunction {
 private:
  const CPPType &m_type;

 public:
  MF_ParticleAttribute(const CPPType &type);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_ParticleIsInGroup final : public MultiFunction {
 public:
  MF_ParticleIsInGroup();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_EmitterTimeInfo final : public MultiFunction {
 public:
  MF_EmitterTimeInfo();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

}  // namespace FN
