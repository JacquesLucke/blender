#pragma once

#include "function.hpp"
#include "source_info.hpp"

#include "BLI_small_set.hpp"
#include "BLI_small_set_vector.hpp"
#include "BLI_mempool.hpp"
#include "BLI_multipool.hpp"
#include "BLI_multimap.hpp"

namespace FN {

class Socket;
class Node;
class GraphLinks;
class DataFlowGraph;

using SocketSet = SmallSet<Socket>;
using NodeSet = SmallSet<Node *>;
using NodeSetVector = SmallSetVector<Node *>;
using SocketVector = SmallVector<Socket>;
using SocketSetVector = SmallSetVector<Socket>;

class Socket {
 public:
  Socket(Node *node, bool is_output, uint index)
      : m_node(node), m_is_output(is_output), m_index(index)
  {
  }

  static inline Socket Input(Node *node, uint index);
  static inline Socket Output(Node *node, uint index);

  inline Node *node() const;
  inline DataFlowGraph *graph() const;

  inline bool is_input() const;
  inline bool is_output() const;
  inline uint index() const;

  SharedType &type() const;
  std::string name() const;

  friend bool operator==(const Socket &a, const Socket &b);
  friend std::ostream &operator<<(std::ostream &stream, Socket socket);

  inline Socket origin() const;
  inline ArrayRef<Socket> targets() const;
  inline bool is_linked() const;

 private:
  Node *m_node;
  bool m_is_output;
  uint m_index;
};

class Node {
 public:
  Node(DataFlowGraph *graph, SharedFunction &function, SourceInfo *source)
      : m_graph(graph), m_function(function), m_source(source)
  {
  }

  ~Node()
  {
    if (m_source != nullptr) {
      /* is allocated in mempool */
      m_source->~SourceInfo();
    }
  }

  Socket input(uint index)
  {
    return Socket::Input(this, index);
  }

  Socket output(uint index)
  {
    return Socket::Output(this, index);
  }

  DataFlowGraph *graph() const
  {
    return m_graph;
  }

  const SharedFunction &function() const
  {
    return m_function;
  }

  SharedFunction &function()
  {
    return m_function;
  }

  const Signature &signature() const
  {
    return this->function()->signature();
  }

  Signature &signature()
  {
    return this->function()->signature();
  }

  uint input_amount() const
  {
    return this->signature().inputs().size();
  }

  uint output_amount() const
  {
    return this->signature().outputs().size();
  }

  SourceInfo *source() const
  {
    return m_source;
  }

  class SocketIterator {
   private:
    Node *m_node;
    bool m_is_output;
    uint m_index;

    SocketIterator(Node *node, bool is_output, uint index)
        : m_node(node), m_is_output(is_output), m_index(index)
    {
    }

   public:
    SocketIterator(Node *node, bool is_output) : SocketIterator(node, is_output, 0)
    {
    }

    using It = SocketIterator;

    It begin() const
    {
      return It(m_node, m_is_output, 0);
    }

    It end() const
    {
      Signature &sig = m_node->signature();
      return It(m_node, m_is_output, (m_is_output) ? sig.outputs().size() : sig.inputs().size());
    }

    It &operator++()
    {
      m_index++;
      return *this;
    }

    bool operator!=(const It &other) const
    {
      return m_index != other.m_index;
    }

    Socket operator*() const
    {
      return Socket(m_node, m_is_output, m_index);
    }
  };

  SocketIterator inputs()
  {
    return SocketIterator(this, false);
  }

  SocketIterator outputs()
  {
    return SocketIterator(this, true);
  }

 private:
  DataFlowGraph *m_graph;
  SharedFunction m_function;
  SourceInfo *m_source;
};

class Link {
 public:
  static Link New(Socket a, Socket b)
  {
    BLI_assert(a.is_input() != b.is_input());
    if (a.is_input()) {
      return Link(b, a);
    }
    else {
      return Link(a, b);
    }
  }

  Socket from() const
  {
    return m_from;
  }

  Socket to() const
  {
    return m_to;
  }

  friend bool operator==(const Link &a, const Link &b)
  {
    return a.m_from == b.m_from && a.m_to == b.m_to;
  }

 private:
  Link(Socket from, Socket to) : m_from(from), m_to(to)
  {
  }

  Socket m_from;
  Socket m_to;
};

class GraphLinks {
 public:
  void insert(Link link)
  {
    Socket from = link.from();
    Socket to = link.to();

    m_origins.add_new(to, from);
    m_targets.add(from, to);
    m_all_links.append(link);
  }

  SmallVector<Link> all_links() const
  {
    return m_all_links;
  }

  Socket get_origin(Socket socket) const
  {
    BLI_assert(socket.is_input());
    return m_origins.lookup(socket);
  }

  ArrayRef<Socket> get_targets(Socket socket) const
  {
    BLI_assert(socket.is_output());
    if (m_targets.contains(socket)) {
      return m_targets.lookup(socket);
    }
    else {
      return ArrayRef<Socket>();
    }
  }

  bool is_linked(Socket socket) const
  {
    if (socket.is_input()) {
      return m_origins.contains(socket);
    }
    else {
      return m_targets.lookup(socket).size() > 0;
    }
  }

 private:
  SmallMap<Socket, Socket> m_origins;
  MultiMap<Socket, Socket> m_targets;
  SmallVector<Link> m_all_links;
};

class DataFlowGraph : public RefCountedBase {
 public:
  DataFlowGraph();
  DataFlowGraph(DataFlowGraph &other) = delete;

  ~DataFlowGraph();

  Node *insert(SharedFunction &function, SourceInfo *source = nullptr);
  void link(Socket a, Socket b);

  inline bool can_modify() const
  {
    return !this->frozen();
  }

  inline bool frozen() const
  {
    return m_frozen;
  }

  void freeze()
  {
    m_frozen = true;
  }

  SmallVector<Link> all_links() const
  {
    return m_links.all_links();
  }

  const SmallSet<Node *> &all_nodes() const
  {
    return m_nodes;
  }

  template<typename T, typename... Args> T *new_source_info(Args &&... args)
  {
    static_assert(std::is_base_of<SourceInfo, T>::value, "");
    void *ptr = m_source_info_pool.allocate(sizeof(T));
    T *source = new (ptr) T(std::forward<Args>(args)...);
    return source;
  }

  std::string to_dot() const;
  void to_dot__clipboard() const;

 private:
  bool m_frozen = false;
  SmallSet<Node *> m_nodes;
  GraphLinks m_links;
  MemPool m_node_pool;
  MemMultiPool m_source_info_pool;

  friend Node;
  friend Socket;
};

using SharedDataFlowGraph = AutoRefCount<DataFlowGraph>;

/* Socket Inline Functions
   ********************************************** */

inline Socket Socket::Input(Node *node, uint index)
{
  BLI_assert(index < node->signature().inputs().size());
  return Socket(node, false, index);
}

inline Socket Socket::Output(Node *node, uint index)
{
  BLI_assert(index < node->signature().outputs().size());
  return Socket(node, true, index);
}

Node *Socket::node() const
{
  return m_node;
}

DataFlowGraph *Socket::graph() const
{
  return this->node()->graph();
}

bool Socket::is_input() const
{
  return !m_is_output;
}

bool Socket::is_output() const
{
  return m_is_output;
}

uint Socket::index() const
{
  return m_index;
}

inline bool operator==(const Socket &a, const Socket &b)
{
  return (a.m_node == b.m_node && a.m_is_output == b.m_is_output && a.m_index == b.m_index);
}

inline std::ostream &operator<<(std::ostream &stream, Socket socket)
{
  stream << "<" << socket.node()->function()->name();
  stream << ", " << ((socket.is_input()) ? "Input" : "Output");
  stream << ":" << socket.index() << ">";
  return stream;
}

Socket Socket::origin() const
{
  return this->graph()->m_links.get_origin(*this);
}

ArrayRef<Socket> Socket::targets() const
{
  return this->graph()->m_links.get_targets(*this);
}

bool Socket::is_linked() const
{
  return this->graph()->m_links.is_linked(*this);
}

} /* namespace FN */

namespace std {
template<> struct hash<FN::Socket> {
  typedef FN::Socket argument_type;
  typedef size_t result_type;

  result_type operator()(argument_type const &v) const noexcept
  {
    size_t h1 = std::hash<FN::Node *>{}(v.node());
    size_t h2 = std::hash<bool>{}(v.is_input());
    size_t h3 = std::hash<uint>{}(v.index());
    return h1 ^ h2 ^ h3;
  }
};
}  // namespace std
