#pragma once

#include "BLI_vector_adaptor.hpp"

#include "function.hpp"
#include "source_info.hpp"
#include "data_graph.hpp"

namespace FN {

using BLI::VectorAdaptor;

class BuilderSocket;
class BuilderInputSocket;
class BuilderOutputSocket;
class BuilderNode;
class DataGraphBuilder;

class BuilderSocket {
 protected:
  BuilderNode *m_node;
  uint m_index;
  bool m_is_input;

  friend BuilderNode;
  friend DataGraphBuilder;

 public:
  BuilderNode *node();
  uint index();

  bool is_input();
  bool is_output();
};

class BuilderInputSocket : public BuilderSocket {
 private:
  uint m_input_id;
  BuilderOutputSocket *m_origin;

  friend BuilderNode;
  friend DataGraphBuilder;

 public:
  uint input_id();
  BuilderOutputSocket *origin();
  StringRef name();
  SharedType &type();
};

class BuilderOutputSocket : public BuilderSocket {
 private:
  uint m_output_id;
  VectorAdaptor<BuilderInputSocket *> m_targets;

  friend BuilderNode;
  friend DataGraphBuilder;

 public:
  uint output_id();
  ArrayRef<BuilderInputSocket *> targets();
  StringRef name();
  SharedType &type();
};

class BuilderNode {
 private:
  DataGraphBuilder *m_builder;
  SharedFunction m_function;
  SourceInfo *m_source_info;
  uint m_id;

  ArrayRef<BuilderInputSocket *> m_inputs;
  ArrayRef<BuilderOutputSocket *> m_outputs;

  friend DataGraphBuilder;
  friend BuilderSocket;

 public:
  DataGraphBuilder &builder()
  {
    return *m_builder;
  }

  SharedFunction &function()
  {
    return m_function;
  }

  uint id()
  {
    return m_id;
  }

  ArrayRef<BuilderInputSocket *> inputs()
  {
    return m_inputs;
  }

  ArrayRef<BuilderOutputSocket *> outputs()
  {
    return m_outputs;
  }

  Vector<DataSocket> built_inputs()
  {
    Vector<DataSocket> sockets;
    sockets.reserve(m_inputs.size());
    for (BuilderInputSocket *socket : m_inputs) {
      sockets.append_unchecked(DataSocket::FromInput(socket->input_id()));
    }
    return sockets;
  }

  Vector<DataSocket> built_outputs()
  {
    Vector<DataSocket> sockets;
    sockets.reserve(m_outputs.size());
    for (BuilderOutputSocket *socket : m_outputs) {
      sockets.append_unchecked(DataSocket::FromOutput(socket->output_id()));
    }
    return sockets;
  }

  BuilderInputSocket *input(uint index)
  {
    return m_inputs[index];
  }

  BuilderOutputSocket *output(uint index)
  {
    return m_outputs[index];
  }

  SourceInfo *source_info()
  {
    return m_source_info;
  }
};

class DataGraphBuilder {
 private:
  Vector<BuilderNode *> m_nodes;
  uint m_link_counter = 0;
  uint m_input_socket_counter = 0;
  uint m_output_socket_counter = 0;
  std::unique_ptr<MonotonicAllocator> m_source_info_allocator;
  MonotonicAllocator m_allocator;

 public:
  DataGraphBuilder();
  DataGraphBuilder(DataGraphBuilder &other) = delete;
  ~DataGraphBuilder();

  SharedDataGraph build();

  BuilderNode *insert_function(SharedFunction function, SourceInfo *source_info = nullptr);
  void insert_link(BuilderOutputSocket *from, BuilderInputSocket *to);

  template<typename T, typename... Args> T *new_source_info(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<SourceInfo, T>::value), "");
    T *source_info = m_source_info_allocator->allocate<T>();
    new (source_info) T(std::forward<Args>(args)...);
    return source_info;
  }

  std::string to_dot();
  void to_dot__clipboard();
};

/* Socket Inline Functions
 ******************************************/

inline BuilderNode *BuilderSocket::node()
{
  return m_node;
}

inline uint BuilderSocket::index()
{
  return m_index;
}

inline bool BuilderSocket::is_input()
{
  return m_is_input;
}

inline bool BuilderSocket::is_output()
{
  return !m_is_input;
}

inline uint BuilderInputSocket::input_id()
{
  return m_input_id;
}

inline BuilderOutputSocket *BuilderInputSocket::origin()
{
  return m_origin;
}

inline StringRef BuilderInputSocket::name()
{
  return m_node->function()->input_name(this->index());
}

inline SharedType &BuilderInputSocket::type()
{
  return m_node->function()->input_type(this->index());
}

inline uint BuilderOutputSocket::output_id()
{
  return m_output_id;
}

inline ArrayRef<BuilderInputSocket *> BuilderOutputSocket::targets()
{
  return m_targets;
}

inline StringRef BuilderOutputSocket::name()
{
  return m_node->function()->output_name(this->index());
}

inline SharedType &BuilderOutputSocket::type()
{
  return m_node->function()->output_type(this->index());
}

}  // namespace FN
