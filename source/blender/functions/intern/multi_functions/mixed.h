#pragma once

#include <functional>

#include "FN_multi_function.h"

namespace FN {

class MF_CombineColor final : public MultiFunction {
 public:
  MF_CombineColor();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_SeparateColor final : public MultiFunction {
 public:
  MF_SeparateColor();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_CombineVector final : public MultiFunction {
 public:
  MF_CombineVector();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_SeparateVector final : public MultiFunction {
 public:
  MF_SeparateVector();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_FloatArraySum final : public MultiFunction {
 public:
  MF_FloatArraySum();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_FloatRange final : public MultiFunction {
 public:
  MF_FloatRange();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_ObjectWorldLocation final : public MultiFunction {
 public:
  MF_ObjectWorldLocation();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_ObjectVertexPositions final : public MultiFunction {
 public:
  MF_ObjectVertexPositions();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_TextLength final : public MultiFunction {
 public:
  MF_TextLength();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_RandomFloat final : public MultiFunction {
 private:
  uint m_seed;

 public:
  MF_RandomFloat(uint seed);
  void call(MFMask mask, MFParams parms, MFContext context) const override;
};

class MF_RandomFloats final : public MultiFunction {
 private:
  uint m_seed;

 public:
  MF_RandomFloats(uint seed);
  void call(MFMask mask, MFParams parms, MFContext context) const override;
};

class MF_RandomVector final : public MultiFunction {
 private:
  uint m_seed;

 public:
  MF_RandomVector(uint seed);
  void call(MFMask mask, MFParams parms, MFContext context) const override;
};

class MF_ContextVertexPosition final : public MultiFunction {
 public:
  MF_ContextVertexPosition();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_ContextCurrentFrame final : public MultiFunction {
 public:
  MF_ContextCurrentFrame();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_SwitchSingle final : public MultiFunction {
 private:
  const CPPType &m_type;

 public:
  MF_SwitchSingle(const CPPType &type);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_SwitchVector final : public MultiFunction {
 private:
  const CPPType &m_type;

 public:
  MF_SwitchVector(const CPPType &type);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_SelectSingle final : public MultiFunction {
 private:
  uint m_inputs;

 public:
  MF_SelectSingle(const CPPType &type, uint inputs);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_SelectVector final : public MultiFunction {
 private:
  uint m_inputs;

 public:
  MF_SelectVector(const CPPType &type, uint inputs);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_PerlinNoise final : public MultiFunction {
 public:
  MF_PerlinNoise();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_MapRange final : public MultiFunction {
 private:
  bool m_clamp;

 public:
  MF_MapRange(bool clamp);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_Clamp final : public MultiFunction {
 private:
  bool m_sort_minmax;

 public:
  MF_Clamp(bool sort_minmax);
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

class MF_FindNonClosePoints final : public MultiFunction {
 public:
  MF_FindNonClosePoints();
  void call(MFMask mask, MFParams params, MFContext context) const override;
};

}  // namespace FN
