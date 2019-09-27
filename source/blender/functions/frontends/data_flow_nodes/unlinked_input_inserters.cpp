#include "FN_llvm.hpp"
#include "FN_types.hpp"
#include "FN_dependencies.hpp"
#include "FN_data_flow_nodes.hpp"

#include "mappings.hpp"

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
        Object *value = (Object *)RNA_pointer_get(&rna, "value").data;
        if (value != nullptr) {
          builder.add_output_objects(i, {value});
        }
      }
    }
  }
};

void DynamicSocketLoader::insert(VTreeDataGraphBuilder &builder,
                                 ArrayRef<VirtualSocket *> unlinked_inputs,
                                 MutableArrayRef<BuilderOutputSocket *> r_new_origins)
{
  auto &socket_loaders = MAPPING_socket_loaders();

  Vector<SocketLoader> loaders;
  Vector<bNodeSocket *> bsockets;
  Vector<bNodeTree *> btrees;

  FunctionBuilder fn_builder;
  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    VirtualSocket *vsocket = unlinked_inputs[i];

    SocketLoader loader = socket_loaders->get_loader(vsocket->idname());
    loaders.append(loader);
    fn_builder.add_output(vsocket->name(), builder.query_socket_type(vsocket));

    bsockets.append(vsocket->bsocket());
    btrees.append(vsocket->btree());
  }

  std::unique_ptr<Function> fn = fn_builder.build("Input Sockets");
  fn->add_body<SocketLoaderBody>(btrees, bsockets, loaders);
  fn->add_body<SocketLoaderDependencies>(btrees, bsockets);

  BuilderNode *node = builder.insert_function(*fn);
  builder.add_resource(std::move(fn), "Owned dynamic socket loader function");
  r_new_origins.copy_from(node->outputs());
}

class ConstantOutput : public TupleCallBody {
 private:
  Tuple *m_tuple;

 public:
  void set_tuple(Tuple *tuple)
  {
    m_tuple = tuple;
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
  Tuple *m_tuple;

 public:
  void set_tuple(Tuple *tuple)
  {
    m_tuple = tuple;
  }

  void build_ir(CodeBuilder &builder,
                CodeInterface &interface,
                const BuildIRSettings &UNUSED(settings)) const override
  {
    TupleMeta &meta = m_tuple->meta();

    for (uint i = 0; i < m_tuple->size(); i++) {
      Type *type = meta.types()[i];
      llvm::Value *value = nullptr;
      if (type == Types::TYPE_float) {
        value = builder.getFloat(m_tuple->get<float>(i));
      }
      else if (type == Types::TYPE_int32) {
        value = builder.getInt32(m_tuple->get<int32_t>(i));
      }
      else if (type == Types::TYPE_float3) {
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
                                   MutableArrayRef<BuilderOutputSocket *> r_new_origins)
{
  auto &socket_loaders = MAPPING_socket_loaders();

  FunctionBuilder fn_builder;
  for (VirtualSocket *vsocket : unlinked_inputs) {
    Type *type = builder.query_socket_type(vsocket);
    fn_builder.add_output(vsocket->name(), type);
  }

  std::unique_ptr<Function> fn = fn_builder.build("Unlinked Inputs");

  std::unique_ptr<TupleMeta> inputs_meta = BLI::make_unique<TupleMeta>(fn->output_types());

  auto inputs_tuple_data_init = BLI::make_unique<Array<char>>(
      inputs_meta->size_of_data_and_init());
  std::unique_ptr<Tuple> inputs_tuple = BLI::make_unique<Tuple>(
      *inputs_meta, (void *)inputs_tuple_data_init->begin());

  ConstantOutput &tuple_call_body = *fn->add_body<ConstantOutput>();
  ConstantOutputGen &build_ir_body = *fn->add_body<ConstantOutputGen>();

  for (uint i = 0; i < unlinked_inputs.size(); i++) {
    VirtualSocket *vsocket = unlinked_inputs[i];
    socket_loaders->load(vsocket, *inputs_tuple, i);
  }

  tuple_call_body.set_tuple(inputs_tuple.get());
  build_ir_body.set_tuple(inputs_tuple.get());

  BuilderNode *node = builder.insert_function(*fn);

  builder.add_resource(std::move(inputs_meta), "Meta information for tuple");
  builder.add_resource(std::move(inputs_tuple_data_init), "Buffer for tuple");
  builder.add_resource(std::move(inputs_tuple), "Tuple containing function inputs");
  builder.add_resource(std::move(fn), "Owned constant input function");

  r_new_origins.copy_from(node->outputs());
}

}  // namespace DataFlowNodes
}  // namespace FN
