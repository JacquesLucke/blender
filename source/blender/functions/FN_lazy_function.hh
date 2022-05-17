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

class LazyFunction;

class LazyFunctionParams {
 private:
  const LazyFunction &fn_;

 public:
  LazyFunctionParams(const LazyFunction &fn) : fn_(fn)
  {
  }

  /**
   * Get a pointer to an input value if the value is available already.
   * If the input is not yet available, request it and return null.
   *
   * The #LazyFunction must leave returned object in an initialized state, but can move from it.
   */
  void *try_get_input_data_ptr(int index, const char *name = nullptr);

  /**
   * Get a pointer to where an output value should be stored.
   * The value at the pointer is in an uninitialized state at first.
   * The #LazyFunction is responsible for initializing the value.
   * After the output has been initialized to its final value, #output_set has to be called.
   */
  void *get_output_data_ptr(int index, const char *name = nullptr);

  /**
   * Call this after the output value is initialized.
   */
  void output_set(int index, const char *name = nullptr);

  /**
   * Can be used to detect which outputs have to be computed.
   */
  ValueUsage get_output_usage(int index, const char *name = nullptr);

  /**
   * Tell the caller of the #LazyFunction that a specific input will definitely not be used.
   * Only an input that was not #ValueUsage::Used can become unused.
   */
  void set_input_unused(int index, const char *name = nullptr);

  template<typename T> T extract_input(int index, const char *name = nullptr);
  template<typename T> const T &get_input(int index, const char *name = nullptr);
  template<typename T> void set_output(int index, T &&value, const char *name = nullptr);

 private:
  virtual void *try_get_input_data_ptr_impl(int index) = 0;
  virtual void *get_output_data_ptr_impl(int index) = 0;
  virtual void output_set_impl(int index) = 0;
  virtual ValueUsage get_output_usage_impl(int index) = 0;
  virtual void set_input_unused_impl(int index) = 0;
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

  virtual std::string name() const;
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

/* -------------------------------------------------------------------- */
/** \name #LazyFunction Inline Methods
 * \{ */

void *LazyFunctionParams::try_get_input_data_ptr(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.input_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->try_get_input_data_ptr_impl(index);
}

void *LazyFunctionParams::get_output_data_ptr(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.output_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->get_output_data_ptr_impl(index);
}

void LazyFunctionParams::output_set(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.output_name(index));
  UNUSED_VARS_NDEBUG(name);
  this->output_set_impl(index);
}

ValueUsage LazyFunctionParams::get_output_usage(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.output_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->get_output_usage_impl(index);
}

void LazyFunctionParams::set_input_unused(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.input_name(index));
  UNUSED_VARS_NDEBUG(name);
  this->set_input_unused_impl(index);
}

template<typename T> T LazyFunctionParams::extract_input(int index, const char *name)
{
#ifdef DEBUG
  const LazyFunctionInput &input = fn_.inputs()[index];
  BLI_assert(input.usage == ValueUsage::Used);
  BLI_assert(input.type->is<T>());
#endif

  void *data = this->try_get_input_data_ptr(index, name);
  BLI_assert(data != nullptr);
  T return_value = std::move(*static_cast<T *>(data));
  return return_value;
}

template<typename T> const T &LazyFunctionParams::get_input(int index, const char *name)
{
#ifdef DEBUG
  const LazyFunctionInput &input = fn_.inputs()[index];
  BLI_assert(input.usage == ValueUsage::Used);
  BLI_assert(input.type->is<T>());
#endif

  const void *data = this->try_get_input_data_ptr(index, name);
  BLI_assert(data != nullptr);
  return *static_cast<const T *>(data);
}

template<typename T> void LazyFunctionParams::set_output(int index, T &&value, const char *name)
{
#ifdef DEBUG
  const LazyFunctionOutput &output = fn_.outputs()[index];
  BLI_assert(output.type->is<T>());
#endif

  using DecayT = std::decay_t<T>;
  void *data = this->get_output_data_ptr(index, name);
  new (data) DecayT(std::forward<T>(value));
  this->output_set(index, name);
}

/** \} */

}  // namespace blender::fn
