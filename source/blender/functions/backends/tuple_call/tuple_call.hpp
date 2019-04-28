#pragma once

#include "tuple.hpp"
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

  SharedTupleMeta &meta_in()
  {
    return m_meta_in;
  }

  SharedTupleMeta &meta_out()
  {
    return m_meta_out;
  }
};

class TupleCallBody : public TupleCallBodyBase {
 public:
  BLI_COMPOSITION_DECLARATION(TupleCallBody);

  inline void call__setup_stack(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const
  {
    TextStackFrame frame(this->owner()->name().c_str());
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

  virtual void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &ctx) const = 0;
};

class LazyState {
 private:
  uint m_entry_count = 0;
  bool m_is_done = false;
  void *m_user_data;
  SmallVector<uint> m_requested_inputs;

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

  const SmallVector<uint> &requested_inputs() const
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

class LazyInTupleCallBody : public TupleCallBodyBase {
 public:
  BLI_COMPOSITION_DECLARATION(LazyInTupleCallBody);

  virtual uint user_data_size() const;
  virtual const SmallVector<uint> &always_required() const;
  virtual void call(Tuple &fn_in,
                    Tuple &fn_out,
                    ExecutionContext &ctx,
                    LazyState &state) const = 0;

  inline void call__setup_stack(Tuple &fn_in,
                                Tuple &fn_out,
                                ExecutionContext &ctx,
                                LazyState &state) const
  {
    TextStackFrame frame(this->owner()->name().c_str());
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

#define FN_TUPLE_CALL_ALLOC_TUPLES(body, name_in, name_out) \
  FN_TUPLE_STACK_ALLOC(name_in, body->meta_in()); \
  FN_TUPLE_STACK_ALLOC(name_out, body->meta_out());
