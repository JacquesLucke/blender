/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_cpp_type.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_vector.hh"

namespace blender::fn {

enum class ValueUsage {
  Used,
  Maybe,
  Unused,
};

class LazyFunctionParams {
 public:
  /**
   * Get a pointer to an input value if the value is available already.
   * If the input is not yet available, request it and return null.
   *
   * The #LazyFunction must leave returned object in an initialized state, but can move from it.
   */
  virtual void *try_get_input_data_ptr(int index) = 0;

  /**
   * Get a pointer to where an output value should be stored.
   * The value at the pointer is in an uninitialized state at first.
   * The #LazyFunction is responsible for initializing the value.
   * After the output has been initialized to its final value, #output_set has to be called.
   */
  virtual void *get_output_data_ptr(int index) = 0;

  /**
   * Call this after the output value is initialized.
   */
  virtual void output_set(int index) = 0;

  /**
   * Can be used to detect which outputs have to be computed.
   */
  virtual ValueUsage get_output_usage(int index) = 0;

  /**
   * Tell the caller of the #LazyFunction that a specific input will definitely not be used.
   * Only an input that was not #ValueUsage::Used can become unused.
   */
  virtual void set_input_unused(int index) = 0;
};

struct LazyFunctionInput {
  const CPPType *type = nullptr;
  ValueUsage usage = ValueUsage::Used;
};

struct LazyFunctionOutput {
  const CPPType *type = nullptr;
};

class LazyFunction {
 protected:
  Vector<LazyFunctionInput> inputs_;
  Vector<LazyFunctionOutput> outputs_;

 public:
  virtual ~LazyFunction() = default;
  virtual void execute(LazyFunctionParams &params) const = 0;

  virtual std::string input_name(int index) const;
  virtual std::string output_name(int index) const;

  Span<LazyFunctionInput> inputs() const;
  Span<LazyFunctionOutput> outputs() const;
};

/* -------------------------------------------------------------------- */
/** \name #LazyFunction Inline Methods
 * \{ */

inline Span<LazyFunctionInput> LazyFunction::inputs() const
{
  return inputs_;
}

inline Span<LazyFunctionOutput> LazyFunction::outputs() const
{
  return outputs_;
}

/** \} */

}  // namespace blender::fn
