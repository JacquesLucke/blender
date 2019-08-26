#pragma once

/**
 * The tuple-call calling convention is the main type of function bodies for the pure C++ backend
 * (without JIT compilation). A function implementing the tuple-call body takes a tuple as input
 * and outputs a tuple containing the computed values.
 */

#include "FN_cpp.hpp"
#include "execution_context.hpp"

namespace FN {

class TupleCallBodyBase : public FunctionBody {
 private:
  SharedTupleMeta m_meta_in;
  SharedTupleMeta m_meta_out;

 protected:
  void owner_init_post() override;

 public:
  virtual ~TupleCallBodyBase(){};

  virtual void init_defaults(Tuple &fn_in) const;

  /**
   * Get the metadata for tuples that this function can take as input.
   */
  SharedTupleMeta &meta_in()
  {
    return m_meta_in;
  }

  /**
   * Get the metadata for tuples that this function can output.
   */
  SharedTupleMeta &meta_out()
  {
    return m_meta_out;
  }

  /**
   * Same as tuple.get<T>(index), but checks if the name is correct in debug builds.
   */
  template<typename T> T get_input(Tuple &tuple, uint index, StringRef expected_name) const
  {
#ifdef DEBUG
    StringRef real_name = this->owner()->input_name(index);
    BLI_assert(real_name == expected_name);
#endif
    UNUSED_VARS_NDEBUG(expected_name);
    return tuple.get<T>(index);
  }
  template<typename T> T get_output(Tuple &tuple, uint index, StringRef expected_name) const
  {
#ifdef DEBUG
    StringRef real_name = this->owner()->output_name(index);
    BLI_assert(real_name == expected_name);
#endif
    UNUSED_VARS_NDEBUG(expected_name);
    return tuple.get<T>(index);
  }
  template<typename T>
  void set_input(Tuple &tuple, uint index, StringRef expected_name, const T &value) const
  {
#ifdef DEBUG
    StringRef real_name = this->owner()->input_name(index);
    BLI_assert(real_name == expected_name);
#endif
    UNUSED_VARS_NDEBUG(expected_name);
    tuple.set<T>(index, value);
  }
  template<typename T>
  void set_output(Tuple &tuple, uint index, StringRef expected_name, const T &value) const
  {
#ifdef DEBUG
    StringRef real_name = this->owner()->output_name(index);
    BLI_assert(real_name == expected_name);
#endif
    UNUSED_VARS_NDEBUG(expected_name);
    tuple.set<T>(index, value);
  }
};

class TupleCallBody : public TupleCallBodyBase {
 public:
  static const uint FUNCTION_BODY_ID = 1;

  /**
   * Calls the function with additional stack frames.
   */
  inline void call__setup_stack(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const
  {
    TextStackFrame frame(this->owner()->name().data());
    ctx.stack().push(&frame);
    this->call(fn_in, fn_out, ctx);
    ctx.stack().pop();
  }

  inline void call__setup_stack(Tuple &fn_in,
                                Tuple &fn_out,
                                ExecutionContext &ctx,
                                StackFrame &extra_frame) const
  {
    ctx.stack().push(&extra_frame);
    this->call__setup_stack(fn_in, fn_out, ctx);
    ctx.stack().pop();
  }

  inline void call__setup_stack(Tuple &fn_in,
                                Tuple &fn_out,
                                ExecutionContext &ctx,
                                SourceInfo *source_info) const
  {
    SourceInfoStackFrame frame(source_info);
    this->call__setup_stack(fn_in, fn_out, ctx, frame);
  }

  inline void call__setup_execution_context(Tuple &fn_in, Tuple &fn_out) const
  {
    ExecutionStack stack;
    ExecutionContext ctx(stack);
    this->call__setup_stack(fn_in, fn_out, ctx);
  }

  /**
   * This function has to be implemented for every tuple-call body. It takes in two references to
   * different tuples and the current execution context.
   *
   * By convention, when the function is called, the ownership of the data in both tuples is this
   * function. That means, that values from fn_in can also be destroyed or relocated if
   * appropriate. If fn_in still contains initialized values when this function ends, they will be
   * destructed.
   *
   * The output tuple fn_out can already contain data beforehand, but can also contain only
   * uninitialized data. When this function ends, it is expected that every element in fn_out is
   * initialized.
   */
  virtual void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const = 0;
};

class LazyState {
 private:
  uint m_entry_count = 0;
  bool m_is_done = false;
  void *m_user_data;
  Vector<uint> m_requested_inputs;

 public:
  LazyState(void *user_data) : m_user_data(user_data)
  {
  }

  void start_next_entry()
  {
    m_entry_count++;
    m_requested_inputs.clear();
  }

  void request_input(uint index)
  {
    m_requested_inputs.append(index);
  }

  void done()
  {
    m_is_done = true;
  }

  const Vector<uint> &requested_inputs() const
  {
    return m_requested_inputs;
  }

  bool is_first_entry() const
  {
    return m_entry_count == 1;
  }

  bool is_done() const
  {
    return m_is_done;
  }

  void *user_data() const
  {
    return m_user_data;
  }
};

/**
 * Similar to the normal tuple-call body, but supports lazy input evaluation. That means, that not
 * all its input have to be computed before it is executed. The call function can request which
 * inputs it needs by e.g. first checking other elements in fn_in.
 *
 * To avoid recomputing the same temporary data multiple times, the function can get a memory
 * buffer of a custom size to store custom data until it is done.
 */
class LazyInTupleCallBody : public TupleCallBodyBase {
 public:
  static const uint FUNCTION_BODY_ID = 2;

  /**
   * Required buffer size for temporary data.
   */
  virtual uint user_data_size() const;

  /**
   * Indices of function inputs that are required in any case. Those elements can be expected to be
   * initialized when call is called for the first time.
   */
  virtual const Vector<uint> &always_required() const;

  /**
   * The ownership semantics are the same as in the normal tuple-call. The only difference is the
   * additional LazyState parameter. With it, other inputs can be requested or the execution of the
   * function can be marked as done.
   */
  virtual void call(Tuple &fn_in,
                    Tuple &fn_out,
                    ExecutionContext &ctx,
                    LazyState &state) const = 0;

  inline void call__setup_stack(Tuple &fn_in,
                                Tuple &fn_out,
                                ExecutionContext &ctx,
                                LazyState &state) const
  {
    TextStackFrame frame(this->owner()->name().data());
    ctx.stack().push(&frame);
    this->call(fn_in, fn_out, ctx, state);
    ctx.stack().pop();
  }

  inline void call__setup_stack(Tuple &fn_in,
                                Tuple &fn_out,
                                ExecutionContext &ctx,
                                LazyState &state,
                                StackFrame &extra_frame) const
  {
    ctx.stack().push(&extra_frame);
    this->call__setup_stack(fn_in, fn_out, ctx, state);
    ctx.stack().pop();
  }

  inline void call__setup_stack(Tuple &fn_in,
                                Tuple &fn_out,
                                ExecutionContext &ctx,
                                LazyState &state,
                                SourceInfo *source_info) const
  {
    SourceInfoStackFrame frame(source_info);
    this->call__setup_stack(fn_in, fn_out, ctx, state, frame);
  }
};

} /* namespace FN */

/**
 * Allocate input and output tuples for a particular tuple-call body.
 */
#define FN_TUPLE_CALL_ALLOC_TUPLES(body, name_in, name_out) \
  FN_TUPLE_STACK_ALLOC(name_in, (body).meta_in().ref()); \
  FN_TUPLE_STACK_ALLOC(name_out, (body).meta_out().ref())
