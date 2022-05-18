/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "BLI_cpp_type.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_vector.hh"

namespace blender::fn {

enum class ValueUsage {
  Used,
  Maybe,
  Unused,
};

class LazyFunction;

class LazyFunctionParams {
 protected:
  const LazyFunction &fn_;
  void *storage_;

 public:
  LazyFunctionParams(const LazyFunction &fn, void *storage) : fn_(fn), storage_(storage)
  {
  }

  /**
   * Get a pointer to an input value if the value is available already.
   *
   * The #LazyFunction must leave returned object in an initialized state, but can move from it.
   */
  void *try_get_input_data_ptr(int index, const char *name = nullptr) const;

  /**
   * Same as #try_get_input_data_ptr, but if the data is not yet available, request it. This makes
   * sure that the data will be available in a future execution of the #LazyFunction.
   */
  void *try_get_input_data_ptr_or_request(int index, const char *name = nullptr);

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

  bool output_was_set(int index, const char *name = nullptr) const;

  /**
   * Can be used to detect which outputs have to be computed.
   */
  ValueUsage get_output_usage(int index, const char *name = nullptr) const;

  /**
   * Tell the caller of the #LazyFunction that a specific input will definitely not be used.
   * Only an input that was not #ValueUsage::Used can become unused.
   */
  void set_input_unused(int index, const char *name = nullptr);

  template<typename T> T extract_input(int index, const char *name = nullptr);
  template<typename T> const T &get_input(int index, const char *name = nullptr);
  template<typename T> void set_output(int index, T &&value, const char *name = nullptr);

  void *storage();
  template<typename T> T &storage();

 private:
  virtual void *try_get_input_data_ptr_impl(int index) const = 0;
  virtual void *try_get_input_data_ptr_or_request_impl(int index) = 0;
  virtual void *get_output_data_ptr_impl(int index) = 0;
  virtual void output_set_impl(int index) = 0;
  virtual bool output_was_set_impl(int index) const = 0;
  virtual ValueUsage get_output_usage_impl(int index) const = 0;
  virtual void set_input_unused_impl(int index) = 0;
};

struct LazyFunctionInput {
  const char *static_name;
  const CPPType *type;
  ValueUsage usage;

  LazyFunctionInput(const char *static_name,
                    const CPPType &type,
                    const ValueUsage usage = ValueUsage::Used)
      : static_name(static_name), type(&type), usage(usage)
  {
  }
};

struct LazyFunctionOutput {
  const char *static_name;
  const CPPType *type = nullptr;

  LazyFunctionOutput(const char *static_name, const CPPType &type)
      : static_name(static_name), type(&type)
  {
  }
};

class LazyFunction {
 protected:
  const char *static_name_ = "Unnamed Function";
  Vector<LazyFunctionInput> inputs_;
  Vector<LazyFunctionOutput> outputs_;

 public:
  virtual ~LazyFunction() = default;

  virtual std::string name() const;
  virtual std::string input_name(int index) const;
  virtual std::string output_name(int index) const;

  virtual void *init_storage(LinearAllocator<> &allocator) const;
  virtual void destruct_storage(void *storage) const;

  Span<LazyFunctionInput> inputs() const;
  Span<LazyFunctionOutput> outputs() const;

  void execute(LazyFunctionParams &params) const;

  bool valid_params_for_execution(const LazyFunctionParams &params) const;

 private:
  virtual void execute_impl(LazyFunctionParams &params) const = 0;
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

inline void LazyFunction::execute(LazyFunctionParams &params) const
{
  BLI_assert(this->valid_params_for_execution(params));
  this->execute_impl(params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LazyFunctionParams Inline Methods
 * \{ */

inline void *LazyFunctionParams::try_get_input_data_ptr(int index, const char *name) const
{
  BLI_assert(name == nullptr || name == fn_.input_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->try_get_input_data_ptr_impl(index);
}

inline void *LazyFunctionParams::try_get_input_data_ptr_or_request(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.input_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->try_get_input_data_ptr_or_request_impl(index);
}

inline void *LazyFunctionParams::get_output_data_ptr(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.output_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->get_output_data_ptr_impl(index);
}

inline void LazyFunctionParams::output_set(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.output_name(index));
  UNUSED_VARS_NDEBUG(name);
  this->output_set_impl(index);
}

inline bool LazyFunctionParams::output_was_set(int index, const char *name) const
{
  BLI_assert(name == nullptr || name == fn_.output_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->output_was_set_impl(index);
}

inline ValueUsage LazyFunctionParams::get_output_usage(int index, const char *name) const
{
  BLI_assert(name == nullptr || name == fn_.output_name(index));
  UNUSED_VARS_NDEBUG(name);
  return this->get_output_usage_impl(index);
}

inline void LazyFunctionParams::set_input_unused(int index, const char *name)
{
  BLI_assert(name == nullptr || name == fn_.input_name(index));
  UNUSED_VARS_NDEBUG(name);
  this->set_input_unused_impl(index);
}

template<typename T> inline T LazyFunctionParams::extract_input(int index, const char *name)
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

template<typename T> inline const T &LazyFunctionParams::get_input(int index, const char *name)
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

template<typename T>
inline void LazyFunctionParams::set_output(int index, T &&value, const char *name)
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

inline void *LazyFunctionParams::storage()
{
  return storage_;
}

template<typename T> inline T &LazyFunctionParams::storage()
{
  return *static_cast<T *>(storage_);
}

/** \} */

}  // namespace blender::fn
