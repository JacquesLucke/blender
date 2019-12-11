#pragma once

#include "FN_multi_function.h"

namespace FN {

class MF_ParticleAttributes final : public MultiFunction {
 private:
  Vector<std::string> m_attribute_names;
  Vector<const CPPType *> m_attribute_types;

 public:
  MF_ParticleAttributes(StringRef attribute_name, const CPPType &attribute_type)
      : MF_ParticleAttributes({attribute_name}, {&attribute_type})
  {
  }

  MF_ParticleAttributes(Vector<std::string> attribute_names,
                        Vector<const CPPType *> attribute_types);
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
