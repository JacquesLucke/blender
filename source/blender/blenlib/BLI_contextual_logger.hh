/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>
#include <mutex>

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_multi_value_map.hh"

namespace blender::contextual_logger {

class ContextualLogger;
class IndexedContextualLogger;

class StoredContextBase {
 protected:
  StoredContextBase *parent_ = nullptr;

  friend IndexedContextualLogger;

 public:
  StoredContextBase(StoredContextBase *parent = nullptr) : parent_(parent)
  {
  }

  virtual ~StoredContextBase() = default;

  virtual uint64_t data_hash() const = 0;
  virtual bool data_is_equal(const StoredContextBase &other) const = 0;
};

template<typename T> class StoredContext : public StoredContextBase {
 private:
  T data_;

 public:
  StoredContext(T data, StoredContextBase *parent = nullptr)
      : StoredContextBase(parent), data_(std::move(data))
  {
  }

  uint64_t data_hash() const override
  {
    return get_default_hash(data_);
  }

  bool data_is_equal(const StoredContextBase &other) const override
  {
    if (const StoredContext<T> *other_typed = dynamic_cast<const StoredContext<T> *>(&other)) {
      return data_ == other_typed->data_;
    }
    return false;
  }
};

class ContextBase {
 protected:
  /* A specific context can only be used with one logger. */
  ContextualLogger *logger_ = nullptr;
  ContextBase *parent_ = nullptr;
  std::atomic<StoredContextBase *> stored_self_;

  friend class LocalContextualLogger;

 public:
  ContextBase(ContextualLogger &logger, ContextBase *parent = nullptr)
      : logger_(&logger), parent_(parent), stored_self_(nullptr)
  {
  }

  virtual ~ContextBase() = default;

  virtual destruct_ptr<StoredContextBase> copy_to_stored(
      LinearAllocator<> &allocator, StoredContextBase *stored_parent) const = 0;
};

template<typename T> class Context final : public ContextBase {
 private:
  T data_;

 public:
  Context(ContextualLogger &logger, ContextBase *parent, T data)
      : ContextBase(logger, parent), data_(std::move(data))
  {
  }

  destruct_ptr<StoredContextBase> copy_to_stored(LinearAllocator<> &allocator,
                                                 StoredContextBase *stored_parent) const override
  {
    return allocator.construct<StoredContext<T>>(data_, stored_parent);
  }
};

class StoredDataBase {
 public:
  virtual ~StoredDataBase() = default;
};

template<typename T> class StoredData final : public StoredDataBase {
 private:
  StoredContextBase *context_;
  T data_;

 public:
  StoredData(StoredContextBase &context, T data) : context_(&context), data_(std::move(data))
  {
  }

  const T data() const
  {
    return data_;
  }
};

class LocalContextualLogger {
 private:
  ContextualLogger &logger_;
  LinearAllocator<> allocator_;
  Vector<destruct_ptr<StoredContextBase>> contexts_;
  Vector<destruct_ptr<StoredDataBase>> logged_data_;

  friend IndexedContextualLogger;

 public:
  LocalContextualLogger(ContextualLogger &logger) : logger_(logger)
  {
  }

  template<typename T> void log(ContextBase &context, T data)
  {
    StoredContextBase &stored_contex = this->store_context(context);
    destruct_ptr<StoredDataBase> logged_data = allocator_.construct<StoredData<T>>(
        stored_contex, std::move(data));
    logged_data_.append(std::move(logged_data));
  }

 private:
  StoredContextBase &store_context(ContextBase &context)
  {
    BLI_assert(&logger_ == context.logger_);
    {
      StoredContextBase *stored_context = context.stored_self_.load(std::memory_order_acquire);
      if (stored_context != nullptr) {
        /* Was stored already. */
        return *stored_context;
      }
    }
    StoredContextBase *stored_parent_context = nullptr;
    if (context.parent_ != nullptr) {
      stored_parent_context = &this->store_context(*context.parent_);
    }
    destruct_ptr<StoredContextBase> stored_context_ptr = context.copy_to_stored(
        allocator_, stored_parent_context);
    StoredContextBase &stored_context = *stored_context_ptr;
    /* It's possible that #context.stored_self_ has been updated by another thread in the
     * mean-time. That is ok. Better update it more than once in rare cases than using a mutex. */
    context.stored_self_.store(&stored_context, std::memory_order_release);
    contexts_.append(std::move(stored_context_ptr));
    return stored_context;
  }
};

class IndexedContextualLogger {
 private:
  Vector<StoredContextBase *> root_contexts_;
  MultiValueMap<StoredContextBase *, StoredContextBase *> children_by_context_;
  MultiValueMap<StoredContextBase *, StoredDataBase *> data_by_context_;

 public:
  IndexedContextualLogger(ContextualLogger &logger);
};

class ContextualLogger {
 private:
  threading::EnumerableThreadSpecific<LocalContextualLogger> local_loggers_;

  mutable std::mutex indexed_logger_mutex_;
  mutable std::unique_ptr<IndexedContextualLogger> indexed_logger_;

  friend IndexedContextualLogger;

 public:
  ContextualLogger() : local_loggers_([this]() { return LocalContextualLogger(*this); })
  {
  }

  LocalContextualLogger &local()
  {
    /* Shouldn't access local loggers after indexing. */
    BLI_assert(!indexed_logger_);
    return local_loggers_.local();
  }

  const IndexedContextualLogger &indexed() const
  {
    std::lock_guard lock{indexed_logger_mutex_};
    if (!indexed_logger_) {
      indexed_logger_ = std::make_unique<IndexedContextualLogger>(
          const_cast<ContextualLogger &>(*this));
    }
    return *indexed_logger_;
  }
};

}  // namespace blender::contextual_logger
