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

class LFUserData {
 public:
  virtual ~LFUserData() = default;
};

class LFParams {
 protected:
  const LazyFunction &fn_;
  void *storage_;

 public:
  /* Todo: Move out of this class. */
  LFUserData *user_data_;

 public:
  LFParams(const LazyFunction &fn, void *storage, LFUserData *user_data);

  /**
   * Get a pointer to an input value if the value is available already.
   *
   * The #LazyFunction must leave returned object in an initialized state, but can move from it.
   */
  void *try_get_input_data_ptr(int index) const;

  /**
   * Same as #try_get_input_data_ptr, but if the data is not yet available, request it. This makes
   * sure that the data will be available in a future execution of the #LazyFunction.
   */
  void *try_get_input_data_ptr_or_request(int index);

  /**
   * Get a pointer to where an output value should be stored.
   * The value at the pointer is in an uninitialized state at first.
   * The #LazyFunction is responsible for initializing the value.
   * After the output has been initialized to its final value, #output_set has to be called.
   */
  void *get_output_data_ptr(int index);

  /**
   * Call this after the output value is initialized.
   */
  void output_set(int index);

  bool output_was_set(int index) const;

  /**
   * Can be used to detect which outputs have to be computed.
   */
  ValueUsage get_output_usage(int index) const;

  /**
   * Tell the caller of the #LazyFunction that a specific input will definitely not be used.
   * Only an input that was not #ValueUsage::Used can become unused.
   */
  void set_input_unused(int index);

  template<typename T> T extract_input(int index);
  template<typename T> const T &get_input(int index);
  template<typename T> void set_output(int index, T &&value);

  void *storage();
  template<typename T> T &storage();

  LFUserData *user_data();
  const LFUserData *user_data() const;

  template<typename T> T *user_data();
  template<typename T> const T *user_data() const;

  void set_default_remaining_outputs();

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

  void execute(LFParams &params) const;

  bool valid_params_for_execution(const LFParams &params) const;

 private:
  virtual void execute_impl(LFParams &params) const = 0;
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

inline void LazyFunction::execute(LFParams &params) const
{
  BLI_assert(this->valid_params_for_execution(params));
  this->execute_impl(params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #LFParams Inline Methods
 * \{ */

inline LFParams::LFParams(const LazyFunction &fn, void *storage, LFUserData *user_data)
    : fn_(fn), storage_(storage), user_data_(user_data)
{
}

inline void *LFParams::try_get_input_data_ptr(int index) const
{
  return this->try_get_input_data_ptr_impl(index);
}

inline void *LFParams::try_get_input_data_ptr_or_request(int index)
{
  return this->try_get_input_data_ptr_or_request_impl(index);
}

inline void *LFParams::get_output_data_ptr(int index)
{
  return this->get_output_data_ptr_impl(index);
}

inline void LFParams::output_set(int index)
{
  this->output_set_impl(index);
}

inline bool LFParams::output_was_set(int index) const
{
  return this->output_was_set_impl(index);
}

inline ValueUsage LFParams::get_output_usage(int index) const
{
  return this->get_output_usage_impl(index);
}

inline void LFParams::set_input_unused(int index)
{
  this->set_input_unused_impl(index);
}

template<typename T> inline T LFParams::extract_input(int index)
{
  void *data = this->try_get_input_data_ptr(index);
  BLI_assert(data != nullptr);
  T return_value = std::move(*static_cast<T *>(data));
  return return_value;
}

template<typename T> inline const T &LFParams::get_input(int index)
{
  const void *data = this->try_get_input_data_ptr(index);
  BLI_assert(data != nullptr);
  return *static_cast<const T *>(data);
}

template<typename T> inline void LFParams::set_output(int index, T &&value)
{
  using DecayT = std::decay_t<T>;
  void *data = this->get_output_data_ptr(index);
  new (data) DecayT(std::forward<T>(value));
  this->output_set(index);
}

inline void *LFParams::storage()
{
  return storage_;
}

template<typename T> inline T &LFParams::storage()
{
  return *static_cast<T *>(storage_);
}

inline LFUserData *LFParams::user_data()
{
  return user_data_;
}

inline const LFUserData *LFParams::user_data() const
{
  return user_data_;
}

template<typename T> inline T *LFParams::user_data()
{
  return dynamic_cast<T *>(user_data_);
}

template<typename T> inline const T *LFParams::user_data() const
{
  return dynamic_cast<const T *>(user_data_);
}

/** \} */

}  // namespace blender::fn
