#pragma once

#include "FN_multi_function.h"

#include <sstream>

#include "BLI_hash.h"
#include "BLI_rand_cxx.h"
#include "BLI_float3.h"

namespace FN {

/**
 * The value buffer passed into the constructor should have a longer lifetime than the
 * function itself.
 */
class MF_GenericConstantValue : public MultiFunction {
 private:
  const void *m_value;

 public:
  MF_GenericConstantValue(const CPPType &type, const void *value);
  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override;

  static void value_to_string(std::stringstream &ss, const CPPType &type, const void *value);
};

/**
 * The passed in buffer has to have a longer lifetime than the function itself.
 */
class MF_GenericConstantVector : public MultiFunction {
 private:
  GenericArrayRef m_array;

 public:
  MF_GenericConstantVector(GenericArrayRef array);
  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override;
};

template<typename T> class MF_ConstantValue : public MultiFunction {
 private:
  T m_value;

 public:
  MF_ConstantValue(T value) : m_value(std::move(value))
  {
    MFSignatureBuilder signature = this->get_builder("Constant " + CPP_TYPE<T>().name());
    std::stringstream ss;
    MF_GenericConstantValue::value_to_string(ss, CPP_TYPE<T>(), (const void *)&m_value);
    signature.single_output<T>(ss.str());

    if (CPP_TYPE<T>() == CPP_TYPE<float>()) {
      uint32_t hash = BLI_hash_int_2d(*(uint *)&m_value, 0);
      signature.operation_hash(hash);
    }
    else if (CPP_TYPE<T>() == CPP_TYPE<int>()) {
      uint32_t hash = BLI_hash_int_2d(*(uint *)&m_value, 1);
      signature.operation_hash(hash);
    }
    else if (CPP_TYPE<T>() == CPP_TYPE<std::string>()) {
      uint32_t hash = BLI_hash_string(((std::string *)&m_value)->c_str());
      signature.operation_hash(hash);
    }
    else if (CPP_TYPE<T>() == CPP_TYPE<BKE::ObjectIDHandle>()) {
      BKE::ObjectIDHandle object_handle = *(BKE::ObjectIDHandle *)&m_value;
      uint32_t hash = object_handle.internal_identifier() ^ BLI_RAND_PER_LINE_UINT32;
      signature.operation_hash(hash);
    }
    else if (CPP_TYPE<T>() == CPP_TYPE<BLI::float3>()) {
      BLI::float3 vector = *(BLI::float3 *)&value;
      uint32_t hash = BLI_hash_int_2d(*(uint *)&vector.x, 0);
      hash = BLI_hash_int_2d(*(uint *)&vector.y, hash);
      hash = BLI_hash_int_2d(*(uint *)&vector.z, hash);
      signature.operation_hash(hash);
    }
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableArrayRef<T> output = params.uninitialized_single_output<T>(0);

    mask.foreach_index([&](uint i) { new (output.begin() + i) T(m_value); });
  }
};

}  // namespace FN
