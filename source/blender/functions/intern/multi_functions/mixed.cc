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
#include "BLI_rand.h"
#include "BLI_kdtree.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_customdata_types.h"

#include "IMB_imbuf_types.h"

#include "BKE_customdata.h"

#include "BKE_mesh_runtime.h"
#include "BKE_deform.h"
#include "BKE_id_data_cache.h"

namespace FN {

using BKE::ImageIDHandle;
using BKE::ObjectIDHandle;
using BLI::float2;
using BLI::float3;
using BLI::float4x4;
using BLI::LargeScopedArray;
using BLI::rgba_b;
using BLI::rgba_f;

MF_CombineColor::MF_CombineColor()
{
  MFSignatureBuilder signature = this->get_builder("Combine Color");
  signature.single_input<float>("R");
  signature.single_input<float>("G");
  signature.single_input<float>("B");
  signature.single_input<float>("A");
  signature.single_output<rgba_f>("Color");
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
  MFSignatureBuilder signature = this->get_builder("Separate Color");
  signature.single_input<rgba_f>("Color");
  signature.single_output<float>("R");
  signature.single_output<float>("G");
  signature.single_output<float>("B");
  signature.single_output<float>("A");
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
  MFSignatureBuilder signature = this->get_builder("Combine Vector");
  signature.single_input<float>("X");
  signature.single_input<float>("Y");
  signature.single_input<float>("Z");
  signature.single_output<float3>("Vector");
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
  MFSignatureBuilder signature = this->get_builder("Separate Vector");
  signature.single_input<float3>("Vector");
  signature.single_output<float>("X");
  signature.single_output<float>("Y");
  signature.single_output<float>("Z");
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

MF_VectorFromValue::MF_VectorFromValue()
{
  MFSignatureBuilder signature = this->get_builder("Vector from Value");
  signature.single_input<float>("Value");
  signature.single_output<float3>("Vector");
}

void MF_VectorFromValue::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float> values = params.readonly_single_input<float>(0, "Value");
  MutableArrayRef<float3> r_vectors = params.uninitialized_single_output<float3>(1, "Vector");

  for (uint i : mask.indices()) {
    float value = values[i];
    r_vectors[i] = {value, value, value};
  }
}

MF_FloatArraySum::MF_FloatArraySum()
{
  MFSignatureBuilder signature = this->get_builder("Float Array Sum");
  signature.vector_input<float>("Array");
  signature.single_output<float>("Sum");
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

MF_FloatRange_Amount_Start_Step::MF_FloatRange_Amount_Start_Step()
{
  MFSignatureBuilder signature = this->get_builder("Float Range");
  signature.single_input<int>("Amount");
  signature.single_input<float>("Start");
  signature.single_input<float>("Step");
  signature.vector_output<float>("Range");
}

void MF_FloatRange_Amount_Start_Step::call(MFMask mask,
                                           MFParams params,
                                           MFContext UNUSED(context)) const
{
  VirtualListRef<int> amounts = params.readonly_single_input<int>(0, "Amount");
  VirtualListRef<float> starts = params.readonly_single_input<float>(1, "Start");
  VirtualListRef<float> steps = params.readonly_single_input<float>(2, "Step");
  auto r_ranges = params.vector_output<float>(3, "Range");

  for (uint index : mask.indices()) {
    uint amount = std::max<int>(0, amounts[index]);
    float start = starts[index];
    float step = steps[index];

    MutableArrayRef<float> range = r_ranges.allocate(index, amount);

    for (int i = 0; i < amount; i++) {
      float value = start + i * step;
      range[i] = value;
    }
  }
}

MF_FloatRange_Amount_Start_Stop::MF_FloatRange_Amount_Start_Stop()
{
  MFSignatureBuilder signature = this->get_builder("Float Range");
  signature.single_input<int>("Amount");
  signature.single_input<float>("Start");
  signature.single_input<float>("Stop");
  signature.vector_output<float>("Range");
}

void MF_FloatRange_Amount_Start_Stop::call(MFMask mask,
                                           MFParams params,
                                           MFContext UNUSED(context)) const
{
  VirtualListRef<int> amounts = params.readonly_single_input<int>(0, "Amount");
  VirtualListRef<float> starts = params.readonly_single_input<float>(1, "Start");
  VirtualListRef<float> stops = params.readonly_single_input<float>(2, "Stop");
  auto r_ranges = params.vector_output<float>(3, "Range");

  for (uint index : mask.indices()) {
    uint amount = std::max<int>(0, amounts[index]);
    float start = starts[index];
    float stop = stops[index];

    if (amount == 0) {
      continue;
    }
    else if (amount == 1) {
      r_ranges.append_single(index, (start + stop) / 2.0f);
    }
    else {
      MutableArrayRef<float> range = r_ranges.allocate(index, amount);

      float step = (stop - start) / (amount - 1);
      for (int i = 0; i < amount; i++) {
        float value = start + i * step;
        range[i] = value;
      }
    }
  }
}

MF_ObjectVertexPositions::MF_ObjectVertexPositions()
{
  MFSignatureBuilder signature = this->get_builder("Object Vertex Positions");
  signature.use_global_context<IDHandleLookup>();
  signature.single_input<ObjectIDHandle>("Object");
  signature.vector_output<float3>("Positions");
}

void MF_ObjectVertexPositions::call(MFMask mask, MFParams params, MFContext context) const
{
  VirtualListRef<ObjectIDHandle> objects = params.readonly_single_input<ObjectIDHandle>(0,
                                                                                        "Object");
  auto positions = params.vector_output<float3>(1, "Positions");

  auto *id_handle_lookup = context.try_find_global<IDHandleLookup>();
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
    LargeScopedArray<float3> coords(mesh->totvert);
    for (uint j = 0; j < mesh->totvert; j++) {
      coords[j] = transform.transform_position(mesh->mvert[j].co);
    }
    positions.extend_single(i, coords);
  }
}

MF_ObjectWorldLocation::MF_ObjectWorldLocation()
{
  MFSignatureBuilder signature = this->get_builder("Object Location");
  signature.use_global_context<IDHandleLookup>();
  signature.single_input<ObjectIDHandle>("Object");
  signature.single_output<float3>("Location");
}

void MF_ObjectWorldLocation::call(MFMask mask, MFParams params, MFContext context) const
{
  auto objects = params.readonly_single_input<ObjectIDHandle>(0, "Object");
  auto r_locations = params.uninitialized_single_output<float3>(1, "Location");

  float3 fallback = {0, 0, 0};

  auto *id_handle_lookup = context.try_find_global<IDHandleLookup>();
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
  MFSignatureBuilder signature = this->get_builder("Switch");
  signature.single_input<bool>("Condition");
  signature.single_input("True", m_type);
  signature.single_input("False", m_type);
  signature.single_output("Result", m_type);
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
  MFSignatureBuilder signature = this->get_builder("Switch");
  signature.single_input<bool>("Condition");
  signature.vector_input("True", m_type);
  signature.vector_input("False", m_type);
  signature.vector_output("Result", m_type);
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

MF_SelectSingle::MF_SelectSingle(const CPPType &type, uint inputs) : m_inputs(inputs)
{
  MFSignatureBuilder signature = this->get_builder("Select Single: " + type.name());
  signature.single_input<int>("Select");
  for (uint i : IndexRange(inputs)) {
    signature.single_input(std::to_string(i), type);
  }
  signature.single_input("Fallback", type);
  signature.single_output("Result", type);
}

void MF_SelectSingle::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<int> selects = params.readonly_single_input<int>(0, "Select");
  GenericVirtualListRef fallbacks = params.readonly_single_input(m_inputs + 1, "Fallback");
  GenericMutableArrayRef r_results = params.uninitialized_single_output(m_inputs + 2, "Result");

  for (uint i : mask.indices()) {
    int select = selects[i];
    if (0 <= select && select < m_inputs) {
      GenericVirtualListRef selected = params.readonly_single_input(select + 1);
      r_results.copy_in__uninitialized(i, selected[i]);
    }
    else {
      r_results.copy_in__uninitialized(i, fallbacks[i]);
    }
  }
}

MF_SelectVector::MF_SelectVector(const CPPType &base_type, uint inputs) : m_inputs(inputs)
{
  MFSignatureBuilder signature = this->get_builder("Select Vector: " + base_type.name() + " List");
  signature.single_input<int>("Select");
  for (uint i : IndexRange(inputs)) {
    signature.vector_input(std::to_string(i), base_type);
  }
  signature.vector_input("Fallback", base_type);
  signature.vector_output("Result", base_type);
}

void MF_SelectVector::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<int> selects = params.readonly_single_input<int>(0, "Select");
  GenericVirtualListListRef fallback = params.readonly_vector_input(m_inputs + 1, "Fallback");
  GenericVectorArray &r_results = params.vector_output(m_inputs + 2, "Result");

  for (uint i : mask.indices()) {
    int select = selects[i];
    if (0 <= select && select < m_inputs) {
      GenericVirtualListListRef selected = params.readonly_vector_input(select + 1);
      r_results.extend_single__copy(i, selected[i]);
    }
    else {
      r_results.extend_single__copy(i, fallback[i]);
    }
  }
}

MF_TextLength::MF_TextLength()
{
  MFSignatureBuilder signature = this->get_builder("Text Length");
  signature.single_input<std::string>("Text");
  signature.single_output<int>("Length");
}

void MF_TextLength::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  auto texts = params.readonly_single_input<std::string>(0, "Text");
  auto lengths = params.uninitialized_single_output<int>(1, "Length");

  for (uint i : mask.indices()) {
    lengths[i] = texts[i].size();
  }
}

MF_ContextVertexPosition::MF_ContextVertexPosition()
{
  MFSignatureBuilder signature = this->get_builder("Vertex Position");
  signature.use_element_context<VertexPositionArray>();
  signature.single_output<float3>("Position");
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
  MFSignatureBuilder signature = this->get_builder("Current Frame");
  signature.use_global_context<SceneTimeContext>();
  signature.single_output<float>("Frame");
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
  MFSignatureBuilder signature = this->get_builder("Perlin Noise");
  signature.single_input<float3>("Position");
  signature.single_input<float>("Amplitude");
  signature.single_input<float>("Scale");
  signature.single_output<float>("Noise 1D");
  signature.single_output<float3>("Noise 3D");
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

MF_MapRange::MF_MapRange(bool clamp) : m_clamp(clamp)
{
  MFSignatureBuilder signature = this->get_builder("Map Range");
  signature.single_input<float>("Value");
  signature.single_input<float>("From Min");
  signature.single_input<float>("From Max");
  signature.single_input<float>("To Min");
  signature.single_input<float>("To Max");
  signature.single_output<float>("Value");
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
  MFSignatureBuilder signature = this->get_builder("Clamp");
  signature.single_input<float>("Value");
  signature.single_input<float>("Min");
  signature.single_input<float>("Max");
  signature.single_output<float>("Value");
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

MF_RandomFloat::MF_RandomFloat(uint seed) : m_seed(seed * 53723457)
{
  MFSignatureBuilder signature = this->get_builder("Random Float");
  signature.single_input<float>("Min");
  signature.single_input<float>("Max");
  signature.single_input<int>("Seed");
  signature.single_output<float>("Value");
}

void MF_RandomFloat::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float> min_values = params.readonly_single_input<float>(0, "Min");
  VirtualListRef<float> max_values = params.readonly_single_input<float>(1, "Max");
  VirtualListRef<int> seeds = params.readonly_single_input<int>(2, "Seed");
  MutableArrayRef<float> r_values = params.uninitialized_single_output<float>(3, "Value");

  for (uint i : mask.indices()) {
    float value = BLI_hash_int_01(seeds[i] ^ m_seed);
    r_values[i] = value * (max_values[i] - min_values[i]) + min_values[i];
  }
}

MF_RandomFloats::MF_RandomFloats(uint seed) : m_seed(seed * 2354567)
{
  MFSignatureBuilder signature = this->get_builder("Random Floats");
  signature.single_input<int>("Amount");
  signature.single_input<float>("Min");
  signature.single_input<float>("Max");
  signature.single_input<int>("Seed");
  signature.vector_output<float>("Values");
}

void MF_RandomFloats::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<int> amounts = params.readonly_single_input<int>(0, "Amount");
  VirtualListRef<float> min_values = params.readonly_single_input<float>(1, "Min");
  VirtualListRef<float> max_values = params.readonly_single_input<float>(2, "Max");
  VirtualListRef<int> seeds = params.readonly_single_input<int>(3, "Seed");
  GenericVectorArray::MutableTypedRef<float> r_values = params.vector_output<float>(4, "Values");

  RNG *rng = BLI_rng_new(0);

  for (uint i : mask.indices()) {
    uint amount = std::max<int>(0, amounts[i]);
    MutableArrayRef<float> r_array = r_values.allocate(i, amount);
    BLI_rng_srandom(rng, seeds[i] + m_seed);

    float range = max_values[i] - min_values[i];
    float offset = min_values[i];

    for (float &r_value : r_array) {
      r_value = BLI_rng_get_float(rng) * range + offset;
    }
  }

  BLI_rng_free(rng);
}

MF_RandomVector::MF_RandomVector(uint seed, RandomVectorMode::Enum mode)
    : m_seed(seed * 56242361), m_mode(mode)
{
  MFSignatureBuilder signature = this->get_builder("Random Vector");
  signature.single_input<float3>("Factor");
  signature.single_input<int>("Seed");
  signature.single_output<float3>("Vector");
}

static float3 rng_get_float3_01(RNG *rng)
{
  float x = BLI_rng_get_float(rng);
  float y = BLI_rng_get_float(rng);
  float z = BLI_rng_get_float(rng);
  return {x, y, z};
}

static float3 rng_get_float3_neg1_1(RNG *rng)
{
  return rng_get_float3_01(rng) * 2 - float3(1.0f, 1.0f, 1.0f);
}

void MF_RandomVector::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<float3> factors = params.readonly_single_input<float3>(0, "Factor");
  VirtualListRef<int> seeds = params.readonly_single_input<int>(1, "Seed");
  MutableArrayRef<float3> r_vectors = params.uninitialized_single_output<float3>(2, "Vector");

  RNG *rng = BLI_rng_new(0);

  switch (m_mode) {
    case RandomVectorMode::UniformInCube: {
      for (uint i : mask.indices()) {
        uint seed = seeds[i] ^ m_seed;
        BLI_rng_srandom(rng, seed);
        float3 vector = rng_get_float3_neg1_1(rng);
        r_vectors[i] = vector * factors[i];
      }
      break;
    }
    case RandomVectorMode::UniformOnSphere: {
      for (uint i : mask.indices()) {
        uint seed = seeds[i] ^ m_seed;
        BLI_rng_srandom(rng, seed);
        float3 vector;
        BLI_rng_get_float_unit_v3(rng, vector);
        r_vectors[i] = vector * factors[i];
      }
      break;
    }
    case RandomVectorMode::UniformInSphere: {
      for (uint i : mask.indices()) {
        uint seed = seeds[i] ^ m_seed;
        BLI_rng_srandom(rng, seed);
        float3 vector;
        do {
          vector = rng_get_float3_neg1_1(rng);
        } while (vector.length_squared() >= 1.0f);
        r_vectors[i] = vector * factors[i];
      }
      break;
    }
  }

  BLI_rng_free(rng);
}

MF_RandomVectors::MF_RandomVectors(uint seed, RandomVectorMode::Enum mode)
    : m_seed(seed * 45621347), m_mode(mode)
{
  MFSignatureBuilder signature = this->get_builder("Random Vectors");
  signature.single_input<int>("Amount");
  signature.single_input<float3>("Factor");
  signature.single_input<int>("Seed");
  signature.vector_output<float3>("Vectors");
}

void MF_RandomVectors::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListRef<int> amounts = params.readonly_single_input<int>(0, "Amount");
  VirtualListRef<float3> factors = params.readonly_single_input<float3>(1, "Factor");
  VirtualListRef<int> seeds = params.readonly_single_input<int>(2, "Seed");
  GenericVectorArray::MutableTypedRef<float3> r_vectors_array = params.vector_output<float3>(
      3, "Vectors");

  RNG *rng = BLI_rng_new(0);

  for (uint index : mask.indices()) {
    uint amount = std::max<int>(0, amounts[index]);
    float3 factor = factors[index];
    uint seed = seeds[index] ^ m_seed;

    MutableArrayRef<float3> r_vectors = r_vectors_array.allocate(index, amount);

    BLI_rng_srandom(rng, seed);

    switch (m_mode) {
      case RandomVectorMode::UniformInCube: {
        for (uint i : IndexRange(amount)) {
          float3 vector = rng_get_float3_neg1_1(rng);
          r_vectors[i] = vector;
        }
        break;
      }
      case RandomVectorMode::UniformOnSphere: {
        for (uint i : IndexRange(amount)) {
          float3 vector;
          BLI_rng_get_float_unit_v3(rng, vector);
          r_vectors[i] = vector;
        }
        break;
      }
      case RandomVectorMode::UniformInSphere: {
        for (uint i : IndexRange(amount)) {
          float3 vector;
          do {
            vector = rng_get_float3_neg1_1(rng);
          } while (vector.length_squared() >= 1.0f);
          r_vectors[i] = vector;
        }
        break;
      }
    }

    for (float3 &vector : r_vectors) {
      vector *= factor;
    }
  }

  BLI_rng_free(rng);
}

MF_FindNonClosePoints::MF_FindNonClosePoints()
{
  MFSignatureBuilder signature = this->get_builder("Remove Close Points");
  signature.vector_input<float3>("Points");
  signature.single_input<float>("Min Distance");
  signature.vector_output<int>("Indices");
}

static BLI_NOINLINE Vector<int> find_non_close_indices(VirtualListRef<float3> points,
                                                       float min_distance)
{
  if (min_distance <= 0.0f) {
    return IndexRange(points.size()).as_array_ref().cast<int>();
  }

  KDTree_3d *kdtree = BLI_kdtree_3d_new(points.size());
  for (uint i : IndexRange(points.size())) {
    BLI_kdtree_3d_insert(kdtree, i, points[i]);
  }

  BLI_kdtree_3d_balance(kdtree);

  LargeScopedArray<bool> keep_index(points.size());
  keep_index.fill(true);

  for (uint i : IndexRange(points.size())) {
    if (!keep_index[i]) {
      continue;
    }

    float3 current_point = points[i];

    struct CBData {
      MutableArrayRef<bool> keep_index_ref;
      uint current_index;
    } cb_data = {keep_index, i};

    BLI_kdtree_3d_range_search_cb(
        kdtree,
        current_point,
        min_distance,
        [](void *user_data, int index, const float *UNUSED(co), float UNUSED(dist_sq)) -> bool {
          CBData &cb_data = *(CBData *)user_data;
          if (index != cb_data.current_index) {
            cb_data.keep_index_ref[index] = false;
          }
          return true;
        },
        (void *)&cb_data);
  }

  BLI_kdtree_3d_free(kdtree);

  Vector<int> indices;
  for (uint i : keep_index.index_iterator()) {
    if (keep_index[i]) {
      indices.append(i);
    }
  }

  return indices;
}

void MF_FindNonClosePoints::call(MFMask mask, MFParams params, MFContext UNUSED(context)) const
{
  VirtualListListRef<float3> points_list = params.readonly_vector_input<float3>(0, "Points");
  VirtualListRef<float> min_distances = params.readonly_single_input<float>(1, "Min Distance");
  GenericVectorArray::MutableTypedRef<int> indices_list = params.vector_output<int>(2, "Indices");

  for (uint i : mask.indices()) {
    Vector<int> filtered_indices = find_non_close_indices(points_list[i], min_distances[i]);
    indices_list.extend_single(i, filtered_indices);
  }
}

}  // namespace FN
