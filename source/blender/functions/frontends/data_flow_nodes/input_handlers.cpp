#include "FN_llvm.hpp"
#include "FN_types.hpp"
#include "FN_dependencies.hpp"
#include "FN_data_flow_nodes.hpp"

#include "inserters.hpp"

namespace FN {
namespace DataFlowNodes {

class SocketLoaderBody : public TupleCallBody {
 private:
  Vector<bNodeTree *> m_btrees;
  Vector<bNodeSocket *> m_bsockets;
  Vector<SocketLoader> m_loaders;

 public:
  SocketLoaderBody(ArrayRef<bNodeTree *> btrees,
                   ArrayRef<bNodeSocket *> bsockets,
                   Vector<SocketLoader> &loaders)
      : m_btrees(btrees), m_bsockets(bsockets), m_loaders(loaders)
  {
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    for (uint i = 0; i < m_bsockets.size(); i++) {
      PointerRNA rna;
      bNodeSocket *bsocket = m_bsockets[i];
      auto loader = m_loaders[i];
      bNodeTree *btree = m_btrees[i];

      RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &rna);
      loader(&rna, fn_out, i);
    }
  }
};

class SocketLoaderDependencies : public DepsBody {
 private:
  Vector<bNodeTree *> m_btrees;
  Vector<bNodeSocket *> m_bsockets;

 public:
  SocketLoaderDependencies(ArrayRef<bNodeTree *> btrees, ArrayRef<bNodeSocket *> bsockets)
      : m_btrees(btrees), m_bsockets(bsockets)
  {
  }

  void build_deps(FunctionDepsBuilder &builder) const
  {
    for (uint i = 0; i < m_bsockets.size(); i++) {
      bNodeSocket *bsocket = m_bsockets[i];
      bNodeTree *btree = m_btrees[i];
      if (STREQ(bsocket->idname, "fn_ObjectSocket")) {
        PointerRNA rna;
        RNA_pointer_create(&btree->id, &RNA_NodeSocket, bsocket, &rna);
        Object *value = (Object *)RNA_pointer_get(&rna, "value").id.data;
        if (value != nullptr) {
          builder.add_output_objects(i, {value});
        }
      }
    }
  }
};

void DynamicSocketLoader::insert(VTreeDataGraphBuilder &builder,
                                 ArrayRef<VirtualSocket *> unlinked_inputs,
                                 ArrayRef<BuilderOutputSocket *> r_new_origins)
{
  auto &socket_loader_map = get_socket_loader_map();

  Vector<SocketLoader> loaders;
  Vector<bNodeSocket *> bsockets;
  Vector<bNodeTree *> btrees;

  FunctionBuilder fn_builder;
  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    VirtualSocket *vsocket = unlinked_inputs[i];

    SocketLoader loader = socket_loader_map.lookup(vsocket->idname());
    loaders.append(loader);
    fn_builder.add_output(vsocket->name(), builder.query_socket_type(vsocket));

    bsockets.append(vsocket->bsocket());
    btrees.append(vsocket->btree());
  }

  auto fn = fn_builder.build("Input Sockets");
  fn->add_body<SocketLoaderBody>(btrees, bsockets, loaders);
  fn->add_body<SocketLoaderDependencies>(btrees, bsockets);
  BuilderNode *node = builder.insert_function(fn);

  r_new_origins.copy_from(node->outputs());
}

class ConstantOutput : public TupleCallBody {
 private:
  std::unique_ptr<Tuple> m_tuple;

 public:
  void set_tuple(std::unique_ptr<Tuple> tuple)
  {
    m_tuple = std::move(tuple);
  }

  void call(Tuple &UNUSED(fn_in), Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    BLI_assert(m_tuple->size() == fn_out.size());
    uint size = m_tuple->size();
    for (uint i = 0; i < size; i++) {
      Tuple::copy_element(*m_tuple, i, fn_out, i);
    }
  }
};

class ConstantOutputGen : public LLVMBuildIRBody {
 private:
  std::unique_ptr<Tuple> m_tuple;

 public:
  void set_tuple(std::unique_ptr<Tuple> tuple)
  {
    m_tuple = std::move(tuple);
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    TupleMeta &meta = m_tuple->meta();
    SharedType &float_type = Types::GET_TYPE_float();
    SharedType &int32_type = Types::GET_TYPE_int32();
    SharedType &float3_type = Types::GET_TYPE_float3();

    for (uint i = 0; i < m_tuple->size(); i++) {
      SharedType &type = meta.types()[i];
      llvm::Value *value = nullptr;
      if (type == float_type) {
        value = builder.getFloat(m_tuple->get<float>(i));
      }
      else if (type == int32_type) {
        value = builder.getInt32(m_tuple->get<int32_t>(i));
      }
      else if (type == float3_type) {
        value = builder.getFloat3(m_tuple->get<float3>(i));
      }
      else {
        void *ptr = m_tuple->element_ptr(i);
        LLVMTypeInfo &type_info = type->extension<LLVMTypeInfo>();
        value = type_info.build_load_ir__copy(builder, builder.getAnyPtr(ptr));
      }
      BLI_assert(value != nullptr);
      interface.set_output(i, value);
    }
  }
};

void ConstantInputsHandler::insert(VTreeDataGraphBuilder &builder,
                                   ArrayRef<VirtualSocket *> unlinked_inputs,
                                   ArrayRef<BuilderOutputSocket *> r_new_origins)
{
  auto &socket_loader_map = get_socket_loader_map();

  FunctionBuilder fn_builder;
  for (VirtualSocket *vsocket : unlinked_inputs) {
    SharedType &type = builder.query_socket_type(vsocket);
    fn_builder.add_output(vsocket->name(), type);
  }
  SharedFunction fn = fn_builder.build("Unlinked Inputs");
  ConstantOutput &tuple_call_body = *fn->add_body<ConstantOutput>();
  ConstantOutputGen &build_ir_body = *fn->add_body<ConstantOutputGen>();

  Tuple *tuple1 = new Tuple(tuple_call_body.meta_out());
  Tuple *tuple2 = new Tuple(tuple_call_body.meta_out());

  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    VirtualSocket *vsocket = unlinked_inputs[i];
    SocketLoader loader = socket_loader_map.lookup(vsocket->idname());
    PointerRNA rna = vsocket->rna();
    loader(&rna, *tuple1, i);
    Tuple::copy_element(*tuple1, i, *tuple2, i);
  }

  tuple_call_body.set_tuple(std::unique_ptr<Tuple>(tuple1));
  build_ir_body.set_tuple(std::unique_ptr<Tuple>(tuple2));

  BuilderNode *node = builder.insert_function(fn);
  r_new_origins.copy_from(node->outputs());
}

}  // namespace DataFlowNodes
}  // namespace FN
