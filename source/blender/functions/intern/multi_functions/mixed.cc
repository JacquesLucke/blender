#include "mixed.h"

#include "FN_generic_array_ref.h"
#include "FN_generic_vector_array.h"
#include "FN_multi_function_common_contexts.h"

#include "BLI_math_cxx.h"
#include "BLI_lazy_init_cxx.h"
#include "BLI_string_map.h"
#include "BLI_array_cxx.h"
#include "BLI_noise.h"
#include "BLI_hash.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_customdata_types.h"

#include "IMB_imbuf_types.h"

#include "BKE_customdata.h"
#include "BKE_surface_hook.h"
#include "BKE_mesh_runtime.h"
#include "BKE_deform.h"
#include "BKE_id_data_cache.h"

namespace FN {

using BKE::ImageIDHandle;
using BKE::ObjectIDHandle;
using BKE::SurfaceHook;
using BLI::float2;
using BLI::float3;
using BLI::float4x4;
using BLI::rgba_b;
using BLI::rgba_f;
using BLI::TemporaryArray;
using BLI::TemporaryVector;

MF_AddFloats::MF_AddFloats()
{
  MFSignatureBuilder signature("Add Floats");
  signature.single_input<float>("A");
  signature.single_input<float>("B");
  signature.single_output<float>("Result");
  this->set_signature(signature);
}

void MF_AddFloats::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto a = params.readonly_single_input<float>(0, "A");
  auto b = params.readonly_single_input<float>(1, "B");
  auto result = params.uninitialized_single_output<float>(2, "Result");

  for (uint i : mask.indices()) {
    result[i] = a[i] + b[i];
  }
}

MF_AddFloat3s::MF_AddFloat3s()
{
  MFSignatureBuilder signature("Add Float3s");
  signature.single_input<float3>("A");
  signature.single_input<float3>("B");
  signature.single_output<float3>("Result");
  this->set_signature(signature);
}

void MF_AddFloat3s::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto a = params.readonly_single_input<float3>(0, "A");
  auto b = params.readonly_single_input<float3>(1, "B");
  auto result = params.uninitialized_single_output<float3>(2, "Result");

  for (uint i : mask.indices()) {
    result[i] = a[i] + b[i];
  }
}

MF_CombineColor::MF_CombineColor()
{
  MFSignatureBuilder signature("Combine Color");
  signature.single_input<float>("R");
  signature.single_input<float>("G");
  signature.single_input<float>("B");
  signature.single_input<float>("A");
  signature.single_output<rgba_f>("Color");
  this->set_signature(signature);
}

void MF_CombineColor::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float> r = params.readonly_single_input<float>(0, "R");
  VirtualListRef<float> g = params.readonly_single_input<float>(1, "G");
  VirtualListRef<float> b = params.readonly_single_input<float>(2, "B");
  VirtualListRef<float> a = params.readonly_single_input<float>(3, "A");
  MutableArrayRef<rgba_f> color = params.uninitialized_single_output<rgba_f>(4, "Color");

  for (uint i : mask.indices()) {
    color[i] = {r[i], g[i], b[i], a[i]};
  }
}

MF_SeparateColor::MF_SeparateColor()
{
  MFSignatureBuilder signature("Separate Color");
  signature.single_input<rgba_f>("Color");
  signature.single_output<float>("R");
  signature.single_output<float>("G");
  signature.single_output<float>("B");
  signature.single_output<float>("A");
  this->set_signature(signature);
}

void MF_SeparateColor::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto color = params.readonly_single_input<rgba_f>(0, "Color");
  auto r = params.uninitialized_single_output<float>(1, "R");
  auto g = params.uninitialized_single_output<float>(2, "G");
  auto b = params.uninitialized_single_output<float>(3, "B");
  auto a = params.uninitialized_single_output<float>(4, "A");

  for (uint i : mask.indices()) {
    rgba_f v = color[i];
    r[i] = v.r;
    g[i] = v.g;
    b[i] = v.b;
    a[i] = v.a;
  }
}

MF_CombineVector::MF_CombineVector()
{
  MFSignatureBuilder signature("Combine Vector");
  signature.single_input<float>("X");
  signature.single_input<float>("Y");
  signature.single_input<float>("Z");
  signature.single_output<float3>("Vector");
  this->set_signature(signature);
}

void MF_CombineVector::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float> x = params.readonly_single_input<float>(0, "X");
  VirtualListRef<float> y = params.readonly_single_input<float>(1, "Y");
  VirtualListRef<float> z = params.readonly_single_input<float>(2, "Z");
  MutableArrayRef<float3> vector = params.uninitialized_single_output<float3>(3, "Vector");

  for (uint i : mask.indices()) {
    vector[i] = {x[i], y[i], z[i]};
  }
}

MF_SeparateVector::MF_SeparateVector()
{
  MFSignatureBuilder signature("Separate Vector");
  signature.single_input<float3>("Vector");
  signature.single_output<float>("X");
  signature.single_output<float>("Y");
  signature.single_output<float>("Z");
  this->set_signature(signature);
}

void MF_SeparateVector::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto vector = params.readonly_single_input<float3>(0, "Vector");
  auto x = params.uninitialized_single_output<float>(1, "X");
  auto y = params.uninitialized_single_output<float>(2, "Y");
  auto z = params.uninitialized_single_output<float>(3, "Z");

  for (uint i : mask.indices()) {
    float3 v = vector[i];
    x[i] = v.x;
    y[i] = v.y;
    z[i] = v.z;
  }
}

MF_VectorDistance::MF_VectorDistance()
{
  MFSignatureBuilder signature("Vector Distance");
  signature.single_input<float3>("A");
  signature.single_input<float3>("A");
  signature.single_output<float>("Distances");
  this->set_signature(signature);
}

void MF_VectorDistance::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto a = params.readonly_single_input<float3>(0, "A");
  auto b = params.readonly_single_input<float3>(1, "B");
  auto distances = params.uninitialized_single_output<float>(2, "Distances");

  for (uint i : mask.indices()) {
    distances[i] = float3::distance(a[i], b[i]);
  }
}

MF_FloatArraySum::MF_FloatArraySum()
{
  MFSignatureBuilder signature("Float Array Sum");
  signature.vector_input<float>("Array");
  signature.single_output<float>("Sum");
  this->set_signature(signature);
}

void MF_FloatArraySum::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto arrays = params.readonly_vector_input<float>(0, "Array");
  MutableArrayRef<float> sums = params.uninitialized_single_output<float>(1, "Sum");

  for (uint i : mask.indices()) {
    float sum = 0.0f;
    VirtualListRef<float> array = arrays[i];
    for (uint j = 0; j < array.size(); j++) {
      sum += array[j];
    }
    sums[i] = sum;
  }
}

MF_FloatRange::MF_FloatRange()
{
  MFSignatureBuilder signature("Float Range");
  signature.single_input<int>("Amount");
  signature.single_input<float>("Start");
  signature.single_input<float>("Step");
  signature.vector_output<float>("Range");
  this->set_signature(signature);
}

void MF_FloatRange::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<int> amounts = params.readonly_single_input<int>(0, "Amount");
  VirtualListRef<float> starts = params.readonly_single_input<float>(1, "Start");
  VirtualListRef<float> steps = params.readonly_single_input<float>(2, "Step");
  auto lists = params.vector_output<float>(3, "Range");

  for (uint i : mask.indices()) {
    int amount = amounts[i];
    float start = starts[i];
    float step = steps[i];

    for (int j = 0; j < amount; j++) {
      float value = start + j * step;
      lists.append_single(i, value);
    }
  }
}

MF_ObjectVertexPositions::MF_ObjectVertexPositions()
{
  MFSignatureBuilder signature{"Object Vertex Positions"};
  signature.single_input<ObjectIDHandle>("Object");
  signature.vector_output<float3>("Positions");
  this->set_signature(signature);
}

void MF_ObjectVertexPositions::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<ObjectIDHandle> objects = params.readonly_single_input<ObjectIDHandle>(0,
                                                                                        "Object");
  auto positions = params.vector_output<float3>(1, "Positions");

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    return;
  }

  for (uint i : mask.indices()) {
    Object *object = id_handle_lookup->lookup(objects[i]);
    if (object == nullptr || object->type != OB_MESH) {
      continue;
    }

    float4x4 transform = object->obmat;

    Mesh *mesh = (Mesh *)object->data;
    TemporaryArray<float3> coords(mesh->totvert);
    for (uint j = 0; j < mesh->totvert; j++) {
      coords[j] = transform.transform_position(mesh->mvert[j].co);
    }
    positions.extend_single(i, coords);
  }
}

MF_GetPositionOnSurface::MF_GetPositionOnSurface()
{
  MFSignatureBuilder signature("Get Position on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_output<float3>("Position");
  this->set_signature(signature);
}

void MF_GetPositionOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<SurfaceHook> locations = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  MutableArrayRef<float3> r_positions = params.uninitialized_single_output<float3>(1, "Position");

  float3 fallback = {0, 0, 0};

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_positions.fill_indices(mask.indices(), fallback);
    return;
  }

  for (uint i : mask.indices()) {
    SurfaceHook location = locations[i];
    if (location.type() != BKE::SurfaceHookType::MeshObject) {
      r_positions[i] = fallback;
      continue;
    }

    Object *object = id_handle_lookup->lookup(location.object_handle());
    if (object == nullptr) {
      r_positions[i] = fallback;
      continue;
    }

    Mesh *mesh = (Mesh *)object->data;
    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
    int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

    if (location.triangle_index() >= triangle_amount) {
      r_positions[i] = fallback;
      continue;
    }

    const MLoopTri &triangle = triangles[location.triangle_index()];
    float3 v1 = mesh->mvert[mesh->mloop[triangle.tri[0]].v].co;
    float3 v2 = mesh->mvert[mesh->mloop[triangle.tri[1]].v].co;
    float3 v3 = mesh->mvert[mesh->mloop[triangle.tri[2]].v].co;

    float3 position;
    interp_v3_v3v3v3(position, v1, v2, v3, location.bary_coords());
    float4x4 local_to_world = object->obmat;
    position = local_to_world.transform_position(position);

    r_positions[i] = position;
  }
}

MF_GetNormalOnSurface::MF_GetNormalOnSurface()
{
  MFSignatureBuilder signature("Get Normal on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_output<float3>("Normal");
  this->set_signature(signature);
}

static float3 short_normal_to_float3(const short normal[3])
{
  return float3(
      (float)normal[0] / 32767.0f, (float)normal[1] / 32767.0f, (float)normal[2] / 32767.0f);
}

void MF_GetNormalOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<SurfaceHook> locations = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  MutableArrayRef<float3> r_normals = params.uninitialized_single_output<float3>(1, "Normal");

  float3 fallback = {0, 0, 1};

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_normals.fill_indices(mask.indices(), fallback);
    return;
  }

  for (uint i : mask.indices()) {
    SurfaceHook location = locations[i];
    if (location.type() != BKE::SurfaceHookType::MeshObject) {
      r_normals[i] = fallback;
      continue;
    }

    Object *object = id_handle_lookup->lookup(location.object_handle());
    if (object == nullptr) {
      r_normals[i] = fallback;
      continue;
    }

    Mesh *mesh = (Mesh *)object->data;
    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
    int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

    if (location.triangle_index() >= triangle_amount) {
      r_normals[i] = fallback;
      continue;
    }

    const MLoopTri &triangle = triangles[location.triangle_index()];
    float3 v1 = short_normal_to_float3(mesh->mvert[mesh->mloop[triangle.tri[0]].v].no);
    float3 v2 = short_normal_to_float3(mesh->mvert[mesh->mloop[triangle.tri[1]].v].no);
    float3 v3 = short_normal_to_float3(mesh->mvert[mesh->mloop[triangle.tri[2]].v].no);

    float3 position;
    interp_v3_v3v3v3(position, v1, v2, v3, location.bary_coords());
    float4x4 local_to_world = object->obmat;
    position = local_to_world.transform_direction(position);

    r_normals[i] = position;
  }
}

MF_GetWeightOnSurface::MF_GetWeightOnSurface(std::string vertex_group_name)
    : m_vertex_group_name(std::move(vertex_group_name))
{
  MFSignatureBuilder signature("Get Weight on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_output<float>("Weight");
  this->set_signature(signature);
}

void MF_GetWeightOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<SurfaceHook> locations = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  MutableArrayRef<float> r_weights = params.uninitialized_single_output<float>(1, "Weight");

  float fallback = 0.0f;

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_weights.fill_indices(mask.indices(), fallback);
    return;
  }

  for (uint i : mask.indices()) {
    SurfaceHook location = locations[i];
    if (location.type() != BKE::SurfaceHookType::MeshObject) {
      r_weights[i] = fallback;
      continue;
    }

    Object *object = id_handle_lookup->lookup(location.object_handle());
    if (object == nullptr) {
      r_weights[i] = fallback;
      continue;
    }

    Mesh *mesh = (Mesh *)object->data;
    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
    int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

    if (location.triangle_index() >= triangle_amount) {
      r_weights[i] = fallback;
      continue;
    }

    const MLoopTri &triangle = triangles[location.triangle_index()];
    uint v1 = mesh->mloop[triangle.tri[0]].v;
    uint v2 = mesh->mloop[triangle.tri[1]].v;
    uint v3 = mesh->mloop[triangle.tri[2]].v;

    MDeformVert *vertex_weights = mesh->dvert;
    int group_index = defgroup_name_index(object, m_vertex_group_name.data());
    if (group_index == -1 || vertex_weights == nullptr) {
      r_weights[i] = fallback;
    }

    float3 corner_weights{defvert_find_weight(vertex_weights + v1, group_index),
                          defvert_find_weight(vertex_weights + v2, group_index),
                          defvert_find_weight(vertex_weights + v3, group_index)};

    float weight = float3::dot(location.bary_coords(), corner_weights);
    r_weights[i] = weight;
  }
}

MF_GetImageColorOnSurface::MF_GetImageColorOnSurface()
{
  MFSignatureBuilder signature("Get Image Color on Surface");
  signature.single_input<SurfaceHook>("Surface Hook");
  signature.single_input<ImageIDHandle>("Image");
  signature.single_output<rgba_f>("Color");
  this->set_signature(signature);
}

template<typename T, typename FuncT, typename EqualFuncT = std::equal_to<T>>
void group_indices_by_same_value(ArrayRef<uint> indices,
                                 VirtualListRef<T> values,
                                 const FuncT &func,
                                 EqualFuncT equal = std::equal_to<T>())
{
  Vector<T> seen_values;

  for (uint i : indices.index_iterator()) {
    uint index = indices[i];

    const T &value = values[index];
    if (seen_values.as_ref().any([&](const T &seen_value) { return equal(value, seen_value); })) {
      continue;
    }
    seen_values.append(value);

    TemporaryVector<uint> indices_with_value;
    for (uint j : indices.drop_front(i)) {
      if (equal(values[j], value)) {
        indices_with_value.append(j);
      }
    }

    func(value, indices_with_value.as_ref());
  }
}

static void get_colors_on_surface(ArrayRef<uint> indices,
                                  VirtualListRef<SurfaceHook> surface_hooks,
                                  MutableArrayRef<rgba_f> r_colors,
                                  rgba_f fallback,
                                  const IDHandleLookup &id_handle_lookup,
                                  const ImBuf &ibuf)
{
  group_indices_by_same_value(
      indices,
      surface_hooks,
      [&](SurfaceHook base_hook, ArrayRef<uint> indices_with_similar_hook) {
        if (base_hook.type() != BKE::SurfaceHookType::MeshObject) {
          r_colors.fill_indices(indices_with_similar_hook, fallback);
          return;
        }

        Object *object = id_handle_lookup.lookup(base_hook.object_handle());
        if (object == nullptr) {
          r_colors.fill_indices(indices_with_similar_hook, fallback);
          return;
        }

        Mesh *mesh = (Mesh *)object->data;
        const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
        int triangle_amount = BKE_mesh_runtime_looptri_len(mesh);

        int uv_layer_index = 0;
        ArrayRef<MLoopUV> uv_layer = BLI::ref_c_array(
            (MLoopUV *)CustomData_get_layer_n(&mesh->ldata, CD_MLOOPUV, uv_layer_index),
            mesh->totloop);

        ArrayRef<rgba_b> pixel_buffer = BLI::ref_c_array((rgba_b *)ibuf.rect, ibuf.x * ibuf.y);

        for (uint i : indices_with_similar_hook) {
          SurfaceHook hook = surface_hooks[i];
          if (hook.triangle_index() >= triangle_amount) {
            r_colors[i] = fallback;
            continue;
          }

          const MLoopTri &triangle = triangles[hook.triangle_index()];

          float2 uv1 = uv_layer[triangle.tri[0]].uv;
          float2 uv2 = uv_layer[triangle.tri[1]].uv;
          float2 uv3 = uv_layer[triangle.tri[2]].uv;

          float2 uv;
          interp_v2_v2v2v2(uv, uv1, uv2, uv3, hook.bary_coords());

          uv = uv.clamped_01();
          uint x = uv.x * (ibuf.x - 1);
          uint y = uv.y * (ibuf.y - 1);
          rgba_b color = pixel_buffer[y * ibuf.x + x];
          r_colors[i] = color;
        }
      },
      [](const SurfaceHook &a, const SurfaceHook &b) {
        if (a.type() != b.type()) {
          return false;
        }
        switch (a.type()) {
          case BKE::SurfaceHookType::MeshObject:
            return a.object_handle() == b.object_handle();
          case BKE::SurfaceHookType::None:
            return true;
        }
        BLI_assert(false);
        return false;
      });
}

void MF_GetImageColorOnSurface::call(MFMask mask, MFParams params, MFContext context) const
{
  if (mask.indices_amount() == 0) {
    return;
  }

  VirtualListRef<SurfaceHook> surface_hooks = params.readonly_single_input<SurfaceHook>(
      0, "Surface Hook");
  VirtualListRef<ImageIDHandle> image_handles = params.readonly_single_input<ImageIDHandle>(
      1, "Image");
  MutableArrayRef<rgba_f> r_colors = params.uninitialized_single_output<rgba_f>(2, "Color");

  rgba_f fallback = {0.0f, 0.0f, 0.0f, 1.0f};

  auto *id_handle_lookup = context.try_find_global<IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_colors.fill_indices(mask.indices(), fallback);
    return;
  }

  group_indices_by_same_value<ImageIDHandle>(
      mask.indices(),
      image_handles,
      [&](ImageIDHandle image_handle, ArrayRef<uint> indices_with_image) {
        Image *image = id_handle_lookup->lookup(image_handle);
        if (image == nullptr) {
          r_colors.fill_indices(indices_with_image, fallback);
          return;
        }

        ImageUser image_user = {0};
        image_user.ok = true;
        ImBuf *ibuf = BKE_image_acquire_ibuf(image, &image_user, NULL);

        get_colors_on_surface(
            indices_with_image, surface_hooks, r_colors, fallback, *id_handle_lookup, *ibuf);

        BKE_image_release_ibuf(image, ibuf, NULL);
      });
}

MF_ObjectWorldLocation::MF_ObjectWorldLocation()
{
  MFSignatureBuilder signature("Object Location");
  signature.single_input<ObjectIDHandle>("Object");
  signature.single_output<float3>("Location");
  this->set_signature(signature);
}

void MF_ObjectWorldLocation::call(MFMask mask, MFParams params, MFContext context) const
{
  auto objects = params.readonly_single_input<ObjectIDHandle>(0, "Object");
  auto r_locations = params.uninitialized_single_output<float3>(1, "Location");

  float3 fallback = {0, 0, 0};

  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();
  if (id_handle_lookup == nullptr) {
    r_locations.fill_indices(mask.indices(), fallback);
    return;
  }

  for (uint i : mask.indices()) {
    Object *object = id_handle_lookup->lookup(objects[i]);
    if (object != nullptr) {
      r_locations[i] = object->obmat[3];
    }
    else {
      r_locations[i] = fallback;
    }
  }
}

MF_SwitchSingle::MF_SwitchSingle(const CPPType &type) : m_type(type)
{
  MFSignatureBuilder signature("Switch");
  signature.single_input<bool>("Condition");
  signature.single_input("True", m_type);
  signature.single_input("False", m_type);
  signature.single_output("Result", m_type);
  this->set_signature(signature);
}

void MF_SwitchSingle::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<bool> conditions = params.readonly_single_input<bool>(0, "Condition");
  GenericVirtualListRef if_true = params.readonly_single_input(1, "True");
  GenericVirtualListRef if_false = params.readonly_single_input(2, "False");
  GenericMutableArrayRef results = params.uninitialized_single_output(3, "Result");

  for (uint i : mask.indices()) {
    if (conditions[i]) {
      results.copy_in__uninitialized(i, if_true[i]);
    }
    else {
      results.copy_in__uninitialized(i, if_false[i]);
    }
  }
}

MF_SwitchVector::MF_SwitchVector(const CPPType &type) : m_type(type)
{
  MFSignatureBuilder signature("Switch");
  signature.single_input<bool>("Condition");
  signature.vector_input("True", m_type);
  signature.vector_input("False", m_type);
  signature.vector_output("Result", m_type);
  this->set_signature(signature);
}

void MF_SwitchVector::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<bool> conditions = params.readonly_single_input<bool>(0, "Condition");
  GenericVirtualListListRef if_true = params.readonly_vector_input(1, "True");
  GenericVirtualListListRef if_false = params.readonly_vector_input(2, "False");
  GenericVectorArray &results = params.vector_output(3, "Result");

  for (uint i : mask.indices()) {
    if (conditions[i]) {
      results.extend_single__copy(i, if_true[i]);
    }
    else {
      results.extend_single__copy(i, if_false[i]);
    }
  }
}

MF_TextLength::MF_TextLength()
{
  MFSignatureBuilder signature("Text Length");
  signature.single_input<std::string>("Text");
  signature.single_output<int>("Length");
  this->set_signature(signature);
}

void MF_TextLength::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto texts = params.readonly_single_input<std::string>(0, "Text");
  auto lengths = params.uninitialized_single_output<int>(1, "Length");

  for (uint i : mask.indices()) {
    lengths[i] = texts[i].size();
  }
}

MF_SimpleVectorize::MF_SimpleVectorize(const MultiFunction &function,
                                       ArrayRef<bool> input_is_vectorized)
    : m_function(function), m_input_is_vectorized(input_is_vectorized)
{
  BLI_assert(input_is_vectorized.contains(true));

  MFSignatureBuilder signature(function.name() + " (Vectorized)");

  bool found_output_param = false;
  UNUSED_VARS_NDEBUG(found_output_param);
  for (uint param_index : function.param_indices()) {
    MFParamType param_type = function.param_type(param_index);
    StringRef param_name = function.param_name(param_index);
    switch (param_type.type()) {
      case MFParamType::VectorInput:
      case MFParamType::VectorOutput:
      case MFParamType::MutableVector:
      case MFParamType::MutableSingle: {
        BLI_assert(false);
        break;
      }
      case MFParamType::SingleInput: {
        BLI_assert(!found_output_param);
        if (input_is_vectorized[param_index]) {
          signature.vector_input(param_name + " (List)",
                                 param_type.data_type().single__cpp_type());
          m_vectorized_inputs.append(param_index);
        }
        else {
          signature.single_input(param_name, param_type.data_type().single__cpp_type());
        }
        break;
      }
      case MFParamType::SingleOutput: {
        signature.vector_output(param_name + " (List)", param_type.data_type().single__cpp_type());
        m_output_indices.append(param_index);
        found_output_param = true;
        break;
      }
    }
  }
  this->set_signature(signature);
}

void MF_SimpleVectorize::call(MFMask mask, MFParams params, MFContext context) const
{
  if (mask.indices_amount() == 0) {
    return;
  }
  uint array_size = mask.min_array_size();

  Vector<int> vectorization_lengths(array_size);
  vectorization_lengths.fill_indices(mask.indices(), -1);

  for (uint param_index : m_vectorized_inputs) {
    GenericVirtualListListRef values = params.readonly_vector_input(param_index,
                                                                    this->param_name(param_index));
    for (uint i : mask.indices()) {
      if (vectorization_lengths[i] != 0) {
        vectorization_lengths[i] = std::max<int>(vectorization_lengths[i], values[i].size());
      }
    }
  }

  Vector<GenericVectorArray *> output_vector_arrays;
  for (uint param_index : m_output_indices) {
    GenericVectorArray *vector_array = &params.vector_output(param_index,
                                                             this->param_name(param_index));
    output_vector_arrays.append(vector_array);
  }

  for (uint index : mask.indices()) {
    uint length = vectorization_lengths[index];
    MFParamsBuilder params_builder(m_function, length);

    for (uint param_index : m_function.param_indices()) {
      MFParamType param_type = m_function.param_type(param_index);
      switch (param_type.type()) {
        case MFParamType::VectorInput:
        case MFParamType::VectorOutput:
        case MFParamType::MutableVector:
        case MFParamType::MutableSingle: {
          BLI_assert(false);
          break;
        }
        case MFParamType::SingleInput: {
          if (m_input_is_vectorized[param_index]) {
            GenericVirtualListListRef input_list_list = params.readonly_vector_input(
                param_index, this->param_name(param_index));
            GenericVirtualListRef repeated_input = input_list_list.repeated_sublist(index, length);
            params_builder.add_readonly_single_input(repeated_input);
          }
          else {
            GenericVirtualListRef input_list = params.readonly_single_input(
                param_index, this->param_name(param_index));
            GenericVirtualListRef repeated_input = input_list.repeated_element(index, length);
            params_builder.add_readonly_single_input(repeated_input);
          }
          break;
        }
        case MFParamType::SingleOutput: {
          GenericVectorArray &output_array_list = params.vector_output(
              param_index, this->param_name(param_index));
          GenericMutableArrayRef output_array = output_array_list.allocate_single(index, length);
          params_builder.add_single_output(output_array);
          break;
        }
      }
    }

    /* TODO: Call with updated context. */
    ArrayRef<uint> sub_mask_indices = IndexRange(length).as_array_ref();
    m_function.call(sub_mask_indices, params_builder, context);
  }
}

MF_ContextVertexPosition::MF_ContextVertexPosition()
{
  MFSignatureBuilder signature("Vertex Position");
  signature.depends_on_per_element_context(true);
  signature.single_output<float3>("Position");
  this->set_signature(signature);
}

void MF_ContextVertexPosition::call(MFMask mask, MFParams params, MFContext context) const
{
  MutableArrayRef<float3> positions = params.uninitialized_single_output<float3>(0, "Position");
  auto vertices_context = context.try_find_per_element<VertexPositionArray>();

  if (vertices_context.has_value()) {
    for (uint i : mask.indices()) {
      uint context_index = vertices_context.value().indices[i];
      positions[i] = vertices_context.value().data->positions[context_index];
    }
  }
  else {
    positions.fill_indices(mask.indices(), {0, 0, 0});
  }
}

MF_ContextCurrentFrame::MF_ContextCurrentFrame()
{
  MFSignatureBuilder signature("Current Frame");
  signature.depends_on_per_element_context(true);
  signature.single_output<float>("Frame");
  this->set_signature(signature);
}

void MF_ContextCurrentFrame::call(MFMask mask, MFParams params, MFContext context) const
{
  MutableArrayRef<float> frames = params.uninitialized_single_output<float>(0, "Frame");

  auto *time_context = context.try_find_global<SceneTimeContext>();

  if (time_context != nullptr) {
    float current_frame = time_context->time;
    frames.fill_indices(mask.indices(), current_frame);
  }
  else {
    frames.fill_indices(mask.indices(), 0.0f);
  }
}

MF_PerlinNoise::MF_PerlinNoise()
{
  MFSignatureBuilder signature("Perlin Noise");
  signature.single_input<float3>("Position");
  signature.single_input<float>("Amplitude");
  signature.single_input<float>("Scale");
  signature.single_output<float>("Noise 1D");
  signature.single_output<float3>("Noise 3D");
  this->set_signature(signature);
}

void MF_PerlinNoise::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float3> positions = params.readonly_single_input<float3>(0, "Position");
  VirtualListRef<float> amplitudes = params.readonly_single_input<float>(1, "Amplitude");
  VirtualListRef<float> scales = params.readonly_single_input<float>(2, "Scale");

  MutableArrayRef<float> r_noise1 = params.uninitialized_single_output<float>(3, "Noise 1D");
  MutableArrayRef<float3> r_noise3 = params.uninitialized_single_output<float3>(4, "Noise 3D");

  for (uint i : mask.indices()) {
    float3 pos = positions[i];
    float noise = BLI_gNoise(scales[i], pos.x, pos.y, pos.z, false, 1);
    r_noise1[i] = noise * amplitudes[i];
  }

  for (uint i : mask.indices()) {
    float3 pos = positions[i];
    float x = BLI_gNoise(scales[i], pos.x, pos.y, pos.z + 1000.0f, false, 1);
    float y = BLI_gNoise(scales[i], pos.x, pos.y + 1000.0f, pos.z, false, 1);
    float z = BLI_gNoise(scales[i], pos.x + 1000.0f, pos.y, pos.z, false, 1);
    r_noise3[i] = float3(x, y, z) * amplitudes[i];
  }
}

MF_ParticleAttributes::MF_ParticleAttributes(Vector<std::string> attribute_names,
                                             Vector<const CPPType *> attribute_types)
    : m_attribute_names(attribute_names), m_attribute_types(attribute_types)
{
  BLI_assert(m_attribute_names.size() == m_attribute_types.size());

  MFSignatureBuilder signature("Particle Attributes");
  signature.depends_on_per_element_context(true);
  for (uint i = 0; i < m_attribute_names.size(); i++) {
    signature.single_output(m_attribute_names[i], *m_attribute_types[i]);
  }
  this->set_signature(signature);
}

void MF_ParticleAttributes::call(MFMask mask, MFParams params, MFContext context) const
{
  auto context_data = context.try_find_per_element<ParticleAttributesContext>();

  for (uint i = 0; i < m_attribute_names.size(); i++) {
    StringRef attribute_name = m_attribute_names[i];
    const CPPType &attribute_type = *m_attribute_types[i];

    GenericMutableArrayRef r_output = params.uninitialized_single_output(0, attribute_name);

    if (context_data.has_value()) {
      AttributesRef attributes = context_data.value().data->attributes;
      Optional<GenericMutableArrayRef> opt_array = attributes.try_get(attribute_name,
                                                                      attribute_type);
      if (opt_array.has_value()) {
        GenericMutableArrayRef array = opt_array.value();
        for (uint i : mask.indices()) {
          attribute_type.copy_to_uninitialized(array[i], r_output[i]);
        }
        return;
      }
    }

    /* Fallback */
    for (uint i : mask.indices()) {
      attribute_type.construct_default(r_output[i]);
    }
  }
}

MF_ParticleIsInGroup::MF_ParticleIsInGroup()
{
  MFSignatureBuilder signature("Particle is in Group");
  signature.depends_on_per_element_context(true);
  signature.single_input<std::string>("Group Name");
  signature.single_output<bool>("Is in Group");
  this->set_signature(signature);
}

void MF_ParticleIsInGroup::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<std::string> group_names = params.readonly_single_input<std::string>(
      0, "Group Name");
  MutableArrayRef<bool> r_is_in_group = params.uninitialized_single_output<bool>(1, "Is in Group");

  auto context_data = context.try_find_per_element<ParticleAttributesContext>();
  if (!context_data.has_value()) {
    r_is_in_group.fill_indices(mask.indices(), false);
    return;
  }

  AttributesRef attributes = context_data->data->attributes;

  for (uint i : mask.indices()) {
    const std::string group_name = group_names[i];
    Optional<MutableArrayRef<bool>> is_in_group_attr = attributes.try_get<bool>(group_name);
    if (!is_in_group_attr.has_value()) {
      r_is_in_group[i] = false;
      continue;
    }

    uint index = context_data->indices[i];
    bool is_in_group = is_in_group_attr.value()[index];
    r_is_in_group[i] = is_in_group;
  }
}

MF_ClosestSurfaceHookOnObject::MF_ClosestSurfaceHookOnObject()
{
  MFSignatureBuilder signature("Closest Point on Object");
  /* Todo: remove this per element dependency. */
  signature.depends_on_per_element_context(true);
  signature.single_input<ObjectIDHandle>("Object");
  signature.single_input<float3>("Position");
  signature.single_output<SurfaceHook>("Closest Location");
  this->set_signature(signature);
}

static BVHTreeNearest get_nearest_point(BVHTreeFromMesh *bvhtree_data, float3 point)
{
  BVHTreeNearest nearest = {0};
  nearest.dist_sq = 10000000.0f;
  nearest.index = -1;
  BLI_bvhtree_find_nearest(
      bvhtree_data->tree, point, &nearest, bvhtree_data->nearest_callback, (void *)bvhtree_data);
  return nearest;
}

static float3 get_barycentric_coords(Mesh *mesh,
                                     const MLoopTri *triangles,
                                     float3 position,
                                     uint triangle_index)
{
  const MLoopTri &triangle = triangles[triangle_index];

  float3 v1 = mesh->mvert[mesh->mloop[triangle.tri[0]].v].co;
  float3 v2 = mesh->mvert[mesh->mloop[triangle.tri[1]].v].co;
  float3 v3 = mesh->mvert[mesh->mloop[triangle.tri[2]].v].co;

  float3 weights;
  interp_weights_tri_v3(weights, v1, v2, v3, position);
  return weights;
}

void MF_ClosestSurfaceHookOnObject::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<ObjectIDHandle> objects = params.readonly_single_input<ObjectIDHandle>(0,
                                                                                        "Object");
  VirtualListRef<float3> positions = params.readonly_single_input<float3>(1, "Position");
  MutableArrayRef<SurfaceHook> r_surface_hooks = params.uninitialized_single_output<SurfaceHook>(
      2, "Closest Location");

  auto *id_data_cache = context.try_find_global<BKE::IDDataCache>();
  auto *id_handle_lookup = context.try_find_global<BKE::IDHandleLookup>();

  if (id_data_cache == nullptr || id_handle_lookup == nullptr) {
    r_surface_hooks.fill_indices(mask.indices(), {});
    return;
  }

  if (mask.indices().size() > 0 && objects.all_equal(mask.indices())) {
    Object *object = id_handle_lookup->lookup(objects[mask.indices()[0]]);
    if (object == nullptr) {
      r_surface_hooks.fill_indices(mask.indices(), {});
      return;
    }

    BVHTreeFromMesh *bvhtree = id_data_cache->get_bvh_tree(object);
    if (bvhtree == nullptr) {
      r_surface_hooks.fill_indices(mask.indices(), {});
      return;
    }

    Mesh *mesh = (Mesh *)object->data;
    const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);
    ObjectIDHandle object_handle(object);

    float4x4 global_to_local = float4x4(object->obmat).inverted__LocRotScale();

    for (uint i : mask.indices()) {
      float3 local_position = global_to_local.transform_position(positions[i]);
      BVHTreeNearest nearest = get_nearest_point(bvhtree, local_position);
      if (nearest.index == -1) {
        r_surface_hooks[i] = {};
        continue;
      }

      float3 bary_coords = get_barycentric_coords(mesh, triangles, nearest.co, nearest.index);
      r_surface_hooks[i] = SurfaceHook(object_handle, nearest.index, bary_coords);
    }
  }
  else {
    for (uint i : mask.indices()) {
      Object *object = id_handle_lookup->lookup(objects[i]);
      if (object == nullptr) {
        r_surface_hooks[i] = {};
        continue;
      }

      BVHTreeFromMesh *bvhtree = id_data_cache->get_bvh_tree(object);
      if (bvhtree == nullptr) {
        r_surface_hooks[i] = {};
        continue;
      }

      Mesh *mesh = (Mesh *)object->data;
      const MLoopTri *triangles = BKE_mesh_runtime_looptri_ensure(mesh);

      float4x4 global_to_local = float4x4(object->obmat).inverted__LocRotScale();
      float3 local_position = global_to_local.transform_position(positions[i]);

      BVHTreeNearest nearest = get_nearest_point(bvhtree, local_position);
      if (nearest.index == -1) {
        r_surface_hooks[i] = {};
        continue;
      }

      float3 bary_coords = get_barycentric_coords(mesh, triangles, nearest.co, nearest.index);
      r_surface_hooks[i] = SurfaceHook(object, nearest.index, bary_coords);
    }
  }
}

MF_MapRange::MF_MapRange(bool clamp) : m_clamp(clamp)
{
  MFSignatureBuilder signature("Map Range");
  signature.single_input<float>("Value");
  signature.single_input<float>("From Min");
  signature.single_input<float>("From Max");
  signature.single_input<float>("To Min");
  signature.single_input<float>("To Max");
  signature.single_output<float>("Value");
  this->set_signature(signature);
}

void MF_MapRange::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float> values = params.readonly_single_input<float>(0, "Value");
  VirtualListRef<float> from_min = params.readonly_single_input<float>(1, "From Min");
  VirtualListRef<float> from_max = params.readonly_single_input<float>(2, "From Max");
  VirtualListRef<float> to_min = params.readonly_single_input<float>(3, "To Min");
  VirtualListRef<float> to_max = params.readonly_single_input<float>(4, "To Max");
  MutableArrayRef<float> r_values = params.uninitialized_single_output<float>(5, "Value");

  for (uint i : mask.indices()) {
    float diff = from_max[i] - from_min[i];
    if (diff != 0.0f) {
      r_values[i] = (values[i] - from_min[i]) / diff * (to_max[i] - to_min[i]) + to_min[i];
    }
    else {
      r_values[i] = to_min[i];
    }
  }

  if (m_clamp) {
    for (uint i : mask.indices()) {
      float min_v = to_min[i];
      float max_v = to_max[i];
      float value = r_values[i];
      if (min_v < max_v) {
        r_values[i] = std::min(std::max(value, min_v), max_v);
      }
      else {
        r_values[i] = std::min(std::max(value, max_v), min_v);
      }
    }
  }
}

MF_Clamp::MF_Clamp(bool sort_minmax) : m_sort_minmax(sort_minmax)
{
  MFSignatureBuilder signature("Clamp");
  signature.single_input<float>("Value");
  signature.single_input<float>("Min");
  signature.single_input<float>("Max");
  signature.single_output<float>("Value");
  this->set_signature(signature);
}

void MF_Clamp::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float> values = params.readonly_single_input<float>(0, "Value");
  VirtualListRef<float> min_values = params.readonly_single_input<float>(1, "Min");
  VirtualListRef<float> max_values = params.readonly_single_input<float>(2, "Max");
  MutableArrayRef<float> r_values = params.uninitialized_single_output<float>(3, "Value");

  if (m_sort_minmax) {
    for (uint i : mask.indices()) {
      float min_v = min_values[i];
      float max_v = max_values[i];
      float value = values[i];
      if (min_v < max_v) {
        r_values[i] = std::min(std::max(value, min_v), max_v);
      }
      else {
        r_values[i] = std::min(std::max(value, max_v), min_v);
      }
    }
  }
  else {
    for (uint i : mask.indices()) {
      float min_v = min_values[i];
      float max_v = max_values[i];
      float value = values[i];
      r_values[i] = std::min(std::max(value, min_v), max_v);
    }
  }
}

MF_RandomFloat::MF_RandomFloat()
{
  MFSignatureBuilder signature("Random Float");
  signature.single_input<int>("Seed");
  signature.single_input<float>("Min");
  signature.single_input<float>("Max");
  signature.single_output<float>("Value");
  this->set_signature(signature);
}

void MF_RandomFloat::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<int> seeds = params.readonly_single_input<int>(0, "Seed");
  VirtualListRef<float> min_values = params.readonly_single_input<float>(1, "Min");
  VirtualListRef<float> max_values = params.readonly_single_input<float>(2, "Max");
  MutableArrayRef<float> r_values = params.uninitialized_single_output<float>(3, "Value");

  for (uint i : mask.indices()) {
    float value = BLI_hash_int_01(seeds[i]);
    r_values[i] = value * (max_values[i] - min_values[i]) + min_values[i];
  }
}

}  // namespace FN
