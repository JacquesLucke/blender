/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_context_stack_map.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_generic_pointer.hh"

#include "BKE_attribute.h"

#include "DNA_node_types.h"

namespace blender::nodes::geo_eval_log {

enum class NodeWarningType {
  Error,
  Warning,
  Info,
};

struct NodeWarning {
  NodeWarningType type;
  std::string message;
};

enum class NamedAttributeUsage {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Remove = 1 << 2,
};
ENUM_OPERATORS(NamedAttributeUsage, NamedAttributeUsage::Remove);

class ValueLog {
 public:
  virtual ~ValueLog() = default;
};

class GenericValueLog : public ValueLog {
 private:
  GMutablePointer data_;

 public:
  GenericValueLog(const GMutablePointer data) : data_(data)
  {
  }

  ~GenericValueLog()
  {
    data_.destruct();
  }

  GPointer value() const
  {
    return data_;
  }
};

struct GeometryAttributeInfo {
  std::string name;
  /** Can be empty when #name does not actually exist on a geometry yet. */
  std::optional<AttributeDomain> domain;
  std::optional<CustomDataType> data_type;
};

class GeoNodesTreeEvalLog {
 private:
  LinearAllocator<> allocator_;
  Vector<destruct_ptr<ValueLog>> logged_values_;
  Map<const bNodeSocket *, ValueLog *> socket_values_;

 public:
  void log_socket_value(const Span<const bNodeSocket *> sockets, const GPointer data)
  {
    const CPPType &type = *data.type();
    void *buffer = allocator_.allocate(type.size(), type.alignment());
    type.copy_construct(data.get(), buffer);
    destruct_ptr<ValueLog> logged_value = allocator_.construct<GenericValueLog>(
        GMutablePointer{type, buffer});
    ValueLog &logged_value_ref = *logged_value;
    for (const bNodeSocket *socket : sockets) {
      socket_values_.add_new(socket, &logged_value_ref);
      std::cout << socket->name << ": " << type.to_string(buffer) << "\n";
    }
    logged_values_.append(std::move(logged_value));
  }

  const ValueLog *try_get_logged_socket_value(const bNodeSocket &socket) const
  {
    return socket_values_.lookup_default(&socket, nullptr);
  }
};

class GeoNodesModifierEvalLog {
 private:
  threading::EnumerableThreadSpecific<ContextStackMap<GeoNodesTreeEvalLog>> logs_;

 public:
  GeoNodesTreeEvalLog &get_local_log(const ContextStack &context_stack)
  {
    return logs_.local().lookup_or_add(context_stack);
  }
};

}  // namespace blender::nodes::geo_eval_log
