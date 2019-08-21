#pragma once

#include "graph_generation.hpp"

namespace FN {
namespace DataFlowNodes {

class DynamicSocketLoader : public UnlinkedInputsInserter {
 public:
  void insert(VTreeDataGraphBuilder &builder,
              ArrayRef<VirtualSocket *> unlinked_inputs,
              MutableArrayRef<BuilderOutputSocket *> r_new_origins) override;
};

class ConstantInputsHandler : public UnlinkedInputsInserter {
 public:
  void insert(VTreeDataGraphBuilder &builder,
              ArrayRef<VirtualSocket *> unlinked_inputs,
              MutableArrayRef<BuilderOutputSocket *> r_new_origins) override;
};

class ReloadableInputs : public UnlinkedInputsInserter {
 private:
  MonotonicAllocator m_allocator;
  Vector<void *> m_addresses;
  Vector<SocketLoader> m_loaders;
  Vector<Type *> m_types;
  Vector<bNodeTree *> m_btrees;
  Vector<bNodeSocket *> m_bsockets;
  std::unique_ptr<Tuple> m_tuple;

  Vector<SharedFunction> m_functions;

 public:
  ~ReloadableInputs();

  void insert(VTreeDataGraphBuilder &builder,
              ArrayRef<VirtualSocket *> unlinked_inputs,
              MutableArrayRef<BuilderOutputSocket *> r_new_origins) override;

  void load();
};

}  // namespace DataFlowNodes
}  // namespace FN
