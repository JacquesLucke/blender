#include "FN_llvm.hpp"
#include "BKE_image.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "IMB_imbuf_types.h"
#include "DNA_customdata_types.h"
#include "BKE_customdata.h"
#include "BKE_mesh_runtime.h"

#include "particle_function_builder.hpp"

#include "events.hpp"
#include "action_contexts.hpp"

namespace BParticles {

using BKE::VirtualSocket;
using BLI::float2;
using BLI::rgba_b;
using FN::DataSocket;
using FN::FunctionBuilder;
using FN::FunctionGraph;
using FN::SharedDataGraph;
using FN::SharedFunction;
using FN::Type;

Vector<DataSocket> find_input_data_sockets(VirtualNode *vnode, VTreeDataGraph &data_graph)
{
  Vector<DataSocket> inputs;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    DataSocket *socket = data_graph.lookup_socket_ptr(vsocket);
    if (socket != nullptr) {
      inputs.append(*socket);
    }
  }
  return inputs;
}

static SetVector<VirtualSocket *> find_particle_dependencies(
    VTreeDataGraph &data_graph,
    ArrayRef<DataSocket> sockets,
    MutableArrayRef<bool> r_depends_on_particle_flags)
{
  SetVector<VirtualSocket *> combined_dependencies;

  for (uint i = 0; i < sockets.size(); i++) {
    DataSocket socket = sockets[i];
    auto dependencies = data_graph.find_placeholder_dependencies({socket});
    bool has_dependency = dependencies.size() > 0;
    r_depends_on_particle_flags[i] = has_dependency;

    combined_dependencies.add_multiple(dependencies);
  }

  return combined_dependencies;
}

class AttributeInputProvider : public ParticleFunctionInputProvider {
 private:
  std::string m_name;

 public:
  AttributeInputProvider(StringRef name) : m_name(name)
  {
  }

  ParticleFunctionInputArray get(InputProviderInterface &interface) override
  {
    AttributeArrays attributes = interface.attributes();
    uint attribute_index = attributes.attribute_index(m_name);
    uint stride = attributes.attribute_stride(attribute_index);
    void *buffer = attributes.get_ptr(attribute_index);
    return {buffer, stride, false};
  }
};

class CollisionNormalInputProvider : public ParticleFunctionInputProvider {
  ParticleFunctionInputArray get(InputProviderInterface &interface) override
  {
    ActionContext *action_context = interface.action_context();
    BLI_assert(action_context != nullptr);
    auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(action_context);
    BLI_assert(surface_info != nullptr);
    return {surface_info->world_normals(), false};
  }
};

class AgeInputProvider : public ParticleFunctionInputProvider {
  ParticleFunctionInputArray get(InputProviderInterface &interface) override
  {
    auto birth_times = interface.attributes().get<float>("Birth Time");
    float *ages_buffer = (float *)BLI_temporary_allocate(sizeof(float) * birth_times.size());
    MutableArrayRef<float> ages(ages_buffer, birth_times.size());

    ParticleTimes &times = interface.particle_times();
    if (times.type() == ParticleTimes::Type::Current) {
      auto current_times = times.current_times();
      for (uint pindex : interface.pindices()) {
        ages[pindex] = current_times[pindex] - birth_times[pindex];
      }
    }
    else if (times.type() == ParticleTimes::Type::DurationAndEnd) {
      auto remaining_durations = times.remaining_durations();
      float end_time = times.end_time();
      for (uint pindex : interface.pindices()) {
        ages[pindex] = end_time - remaining_durations[pindex] - birth_times[pindex];
      }
    }
    else {
      BLI_assert(false);
    }
    return {ArrayRef<float>(ages), true};
  }
};

class SurfaceImageInputProvider : public ParticleFunctionInputProvider {
 private:
  Image *m_image;
  ImageUser m_image_user;
  ImBuf *m_ibuf;

 public:
  SurfaceImageInputProvider(Image *image) : m_image(image)
  {
    memset(&m_image_user, 0, sizeof(ImageUser));
    m_image_user.ok = true;
    m_ibuf = BKE_image_acquire_ibuf(image, &m_image_user, NULL);
    BLI_assert(m_ibuf);
  }

  ~SurfaceImageInputProvider()
  {
    BKE_image_release_ibuf(m_image, m_ibuf, NULL);
  }

  ParticleFunctionInputArray get(InputProviderInterface &interface) override
  {
    ActionContext *action_context = interface.action_context();
    BLI_assert(action_context != nullptr);
    auto *surface_info = dynamic_cast<MeshSurfaceActionContext *>(action_context);
    BLI_assert(surface_info != nullptr);

    const Object *object = surface_info->object();
    Mesh *mesh = (Mesh *)object->data;

    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);

    int uv_layer_index = CustomData_get_active_layer(&mesh->ldata, CD_MLOOPUV);
    BLI_assert(uv_layer_index >= 0);
    MLoopUV *uv_layer = (MLoopUV *)CustomData_get(&mesh->ldata, uv_layer_index, CD_MLOOPUV);
    BLI_assert(uv_layer != nullptr);

    ArrayRef<float3> local_positions = surface_info->local_positions();

    rgba_b *pixel_buffer = (rgba_b *)m_ibuf->rect;

    rgba_f *colors_buffer = (rgba_f *)BLI_temporary_allocate(sizeof(rgba_f) *
                                                             local_positions.size());
    MutableArrayRef<rgba_f> colors{colors_buffer, local_positions.size()};

    for (uint pindex : interface.pindices()) {
      float3 local_position = local_positions[pindex];

      uint triangle_index = surface_info->looptri_indices()[pindex];
      const MLoopTri &triangle = triangles[triangle_index];

      uint loop1 = triangle.tri[0];
      uint loop2 = triangle.tri[1];
      uint loop3 = triangle.tri[2];

      float3 v1 = mesh->mvert[mesh->mloop[loop1].v].co;
      float3 v2 = mesh->mvert[mesh->mloop[loop2].v].co;
      float3 v3 = mesh->mvert[mesh->mloop[loop3].v].co;

      float2 uv1 = uv_layer[loop1].uv;
      float2 uv2 = uv_layer[loop2].uv;
      float2 uv3 = uv_layer[loop3].uv;

      float3 vertex_weights;
      interp_weights_tri_v3(vertex_weights, v1, v2, v3, local_position);

      float2 uv;
      interp_v2_v2v2v2(uv, uv1, uv2, uv3, vertex_weights);
      uv = uv.clamped_01();
      uint x = uv.x * (m_ibuf->x - 1);
      uint y = uv.y * (m_ibuf->y - 1);
      colors[pindex] = pixel_buffer[y * m_ibuf->x + x];
    }
    return {ArrayRef<rgba_f>(colors), true};
  }
};

static ParticleFunctionInputProvider *create_input_provider(VirtualSocket *vsocket)
{
  VirtualNode *vnode = vsocket->vnode();
  if (STREQ(vnode->idname(), "bp_ParticleInfoNode")) {
    if (STREQ(vsocket->name(), "Age")) {
      return new AgeInputProvider();
    }
    else {
      return new AttributeInputProvider(vsocket->name());
    }
  }
  else if (STREQ(vnode->idname(), "bp_CollisionInfoNode")) {
    return new CollisionNormalInputProvider();
  }
  else if (STREQ(vnode->idname(), "bp_SurfaceImageNode")) {
    PointerRNA rna = vnode->rna();
    Image *image = (Image *)RNA_pointer_get(&rna, "image").data;
    BLI_assert(image != nullptr);
    return new SurfaceImageInputProvider(image);
  }
  else {
    BLI_assert(false);
    return nullptr;
  }
}

static SharedFunction create_function__with_deps(
    VTreeDataGraph &data_graph,
    StringRef function_name,
    ArrayRef<DataSocket> sockets_to_compute,
    ArrayRef<VirtualSocket *> input_vsockets,
    MutableArrayRef<ParticleFunctionInputProvider *> r_input_providers)
{
  uint input_amount = input_vsockets.size();
  BLI_assert(input_amount == r_input_providers.size());

  Vector<DataSocket> input_sockets = data_graph.lookup_sockets(input_vsockets);

  FunctionBuilder fn_builder;
  fn_builder.add_inputs(data_graph.graph(), input_sockets);
  fn_builder.add_outputs(data_graph.graph(), sockets_to_compute);

  for (uint i = 0; i < input_amount; i++) {
    r_input_providers[i] = create_input_provider(input_vsockets[i]);
  }

  SharedFunction fn = fn_builder.build(function_name);
  FunctionGraph fgraph(data_graph.graph(), input_sockets, sockets_to_compute);
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  FN::fgraph_add_LLVMBuildIRBody(fn, fgraph);
  return fn;
}

static SharedFunction create_function__without_deps(SharedDataGraph &graph,
                                                    StringRef function_name,
                                                    ArrayRef<DataSocket> sockets_to_compute)
{
  FunctionBuilder fn_builder;
  fn_builder.add_outputs(graph, sockets_to_compute);
  SharedFunction fn = fn_builder.build(function_name);
  FunctionGraph fgraph(graph, {}, sockets_to_compute);
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static ValueOrError<std::unique_ptr<ParticleFunction>> create_particle_function_from_sockets(
    VTreeDataGraph &data_graph,
    StringRef name,
    ArrayRef<DataSocket> sockets_to_compute,
    ArrayRef<bool> depends_on_particle_flags,
    ArrayRef<VirtualSocket *> dependencies)
{
  Vector<DataSocket> sockets_with_deps;
  Vector<DataSocket> sockets_without_deps;
  for (uint i = 0; i < sockets_to_compute.size(); i++) {
    if (depends_on_particle_flags[i]) {
      sockets_with_deps.append(sockets_to_compute[i]);
    }
    else {
      sockets_without_deps.append(sockets_to_compute[i]);
    }
  }

  Vector<ParticleFunctionInputProvider *> input_providers(dependencies.size(), nullptr);

  SharedFunction fn_without_deps = create_function__without_deps(
      data_graph.graph(), name, sockets_without_deps);
  SharedFunction fn_with_deps = create_function__with_deps(
      data_graph, name, sockets_with_deps, dependencies, input_providers);

  ParticleFunction *particle_fn = new ParticleFunction(
      fn_without_deps, fn_with_deps, input_providers, depends_on_particle_flags);
  return std::unique_ptr<ParticleFunction>(particle_fn);
}

ValueOrError<std::unique_ptr<ParticleFunction>> create_particle_function(
    VirtualNode *vnode, VTreeDataGraph &data_graph)
{
  Vector<DataSocket> sockets_to_compute = find_input_data_sockets(vnode, data_graph);
  Vector<bool> depends_on_particle_flags(sockets_to_compute.size());
  auto dependencies = find_particle_dependencies(
      data_graph, sockets_to_compute, depends_on_particle_flags);

  std::string name = vnode->name() + StringRef(" Inputs");
  return create_particle_function_from_sockets(
      data_graph, name, sockets_to_compute, depends_on_particle_flags, dependencies);
}

}  // namespace BParticles
