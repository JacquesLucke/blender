#include "object_input.hpp"
#include "FN_types.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"

#include "BLI_lazy_init.hpp"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

namespace FN {
namespace Functions {

using namespace Types;

class ObjectLocation : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    Object *object = fn_in.get<Object *>(0);
    if (object) {
      float3 position = object->loc;
      fn_out.set<float3>(0, position);
    }
    else {
      fn_out.set<float3>(0, float3());
    }
  }
};

class ObjectLocationDeps : public DepsBody {
  void build_deps(FunctionDepsBuilder &builder) const
  {
    auto objects = builder.get_input_objects(0);
    builder.add_transform_dependency(objects);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_object_location)
{
  FunctionBuilder builder;
  builder.add_input("Object", GET_TYPE_object());
  builder.add_output("Location", GET_TYPE_float3());
  auto fn = builder.build("Object Location");
  fn->add_body<ObjectLocation>();
  fn->add_body<ObjectLocationDeps>();
  return fn;
}

class ObjectMeshVertices : public TupleCallBody {
  void call(Tuple &fn_in, Tuple &fn_out, ExecutionContext &UNUSED(ctx)) const override
  {
    Object *object = fn_in.get<Object *>(0);
    if (object == nullptr || object->type != OB_MESH) {
      auto empty_list = SharedList::New(GET_TYPE_float());
      fn_out.move_in(0, empty_list);
      return;
    }

    Mesh *mesh = (Mesh *)object->data;

    auto vertices = SharedList::New(GET_TYPE_float3());
    vertices->reserve_and_set_size(mesh->totvert);

    ArrayRef<float3> vertices_ref = vertices->as_array_ref<float3>();

    float4x4 transform = object->obmat;

    for (uint i = 0; i < mesh->totvert; i++) {
      vertices_ref[i] = transform.transform_position(mesh->mvert[i].co);
    }
    fn_out.move_in(0, vertices);
  }
};

class ObjectMeshDeps : public DepsBody {
  void build_deps(FunctionDepsBuilder &builder) const
  {
    auto objects = builder.get_input_objects(0);
    builder.add_geometry_dependency(objects);
    builder.add_transform_dependency(objects);
  }
};

BLI_LAZY_INIT(SharedFunction, GET_FN_object_mesh_vertices)
{
  FunctionBuilder builder;
  builder.add_input("Object", GET_TYPE_object());
  builder.add_output("Vertex Locations", GET_TYPE_float3_list());
  auto fn = builder.build("Object Mesh Vertices");
  fn->add_body<ObjectMeshVertices>();
  fn->add_body<ObjectMeshDeps>();
  return fn;
}

}  // namespace Functions
}  // namespace FN
