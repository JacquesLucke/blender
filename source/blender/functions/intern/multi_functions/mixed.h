#pragma once

#include <functional>

#include "FN_multi_function.h"

namespace FN {

class MF_CombineColor final : public MultiFunction {
 public:
  MF_CombineColor();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_SeparateColor final : public MultiFunction {
 public:
  MF_SeparateColor();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_CombineVector final : public MultiFunction {
 public:
  MF_CombineVector();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_SeparateVector final : public MultiFunction {
 public:
  MF_SeparateVector();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_VectorFromValue final : public MultiFunction {
 public:
  MF_VectorFromValue();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_FloatArraySum final : public MultiFunction {
 public:
  MF_FloatArraySum();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_FloatRange_Amount_Start_Step final : public MultiFunction {
 public:
  MF_FloatRange_Amount_Start_Step();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_FloatRange_Amount_Start_Stop final : public MultiFunction {
 public:
  MF_FloatRange_Amount_Start_Stop();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_ObjectWorldLocation final : public MultiFunction {
 public:
  MF_ObjectWorldLocation();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_ObjectVertexPositions final : public MultiFunction {
 public:
  MF_ObjectVertexPositions();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_TextLength final : public MultiFunction {
 public:
  MF_TextLength();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_RandomFloat final : public MultiFunction {
 private:
  uint m_seed;

 public:
  MF_RandomFloat(uint seed);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_RandomFloats final : public MultiFunction {
 private:
  uint m_seed;

 public:
  MF_RandomFloats(uint seed);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

namespace RandomVectorMode {
enum Enum {
  UniformInCube,
  UniformOnSphere,
  UniformInSphere,
};
}

class MF_RandomVector final : public MultiFunction {
 private:
  uint m_seed;
  RandomVectorMode::Enum m_mode;

 public:
  MF_RandomVector(uint seed, RandomVectorMode::Enum mode);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_RandomVectors final : public MultiFunction {
 private:
  uint m_seed;
  RandomVectorMode::Enum m_mode;

 public:
  MF_RandomVectors(uint seed, RandomVectorMode::Enum mode);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_ContextVertexPosition final : public MultiFunction {
 public:
  MF_ContextVertexPosition();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_ContextCurrentFrame final : public MultiFunction {
 public:
  MF_ContextCurrentFrame();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_SwitchSingle final : public MultiFunction {
 private:
  const CPPType &m_type;

 public:
  MF_SwitchSingle(const CPPType &type);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_SwitchVector final : public MultiFunction {
 private:
  const CPPType &m_type;

 public:
  MF_SwitchVector(const CPPType &type);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_SelectSingle final : public MultiFunction {
 private:
  uint m_inputs;

 public:
  MF_SelectSingle(const CPPType &type, uint inputs);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_SelectVector final : public MultiFunction {
 private:
  uint m_inputs;

 public:
  MF_SelectVector(const CPPType &type, uint inputs);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_PerlinNoise final : public MultiFunction {
 public:
  MF_PerlinNoise();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_MapRange final : public MultiFunction {
 private:
  bool m_clamp;

 public:
  MF_MapRange(bool clamp);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_Clamp final : public MultiFunction {
 private:
  bool m_sort_minmax;

 public:
  MF_Clamp(bool sort_minmax);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_FindNonClosePoints final : public MultiFunction {
 public:
  MF_FindNonClosePoints();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

class MF_JoinTextList final : public MultiFunction {
 public:
  MF_JoinTextList();
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

}  // namespace FN
