/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_axis_to_euler_cc {

NODE_STORAGE_FUNCS(NodeFunctionAxisToEuler)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Primary Axis")).hide_value();
  b.add_input<decl::Vector>(N_("Secondary Axis")).hide_value();
  b.add_output<decl::Vector>(N_("Rotation"));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_cnew<NodeFunctionAxisToEuler>(__func__);
  storage->primary_axis = 2;
  storage->secondary_axis = 0;
  node->storage = storage;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNode &node = *static_cast<const bNode *>(ptr->data);
  const NodeFunctionAxisToEuler &storage = node_storage(node);
  if (storage.legacy_distribute_node_behavior) {
    uiItemR(layout, ptr, "legacy_distribute_node_behavior", 0, "Legacy Behavior", ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "primary_axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "secondary_axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

    if (storage.primary_axis == storage.secondary_axis) {
      uiItemL(layout, N_("Must not be equal"), ICON_ERROR);
    }
  }
}

static void node_layout_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "legacy_distribute_node_behavior", 0, "Legacy Behavior", ICON_NONE);
}

static void node_update(bNodeTree *tree, bNode *node)
{
  const NodeFunctionAxisToEuler &storage = node_storage(*node);
  bNodeSocket *primary_axis_socket = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *secondary_axis_socket = primary_axis_socket->next;
  nodeSetSocketAvailability(tree, secondary_axis_socket, !storage.legacy_distribute_node_behavior);
}

static float3 get_orthogonal_of_non_zero_vector(const float3 &v)
{
  BLI_assert(!math::is_zero(v));
  if (v.x != -v.y) {
    return float3{-v.y, v.x, 0.0f};
  }
  if (v.x != -v.z) {
    return float3(-v.z, 0.0f, v.x);
  }
  return {0.0f, -v.z, v.y};
}

class AxisToEulerFunction : public mf::MultiFunction {
 private:
  int primary_axis_;
  int secondary_axis_;
  int tertiary_axis_;

 public:
  AxisToEulerFunction(const int primary_axis, const int secondary_axis)
      : primary_axis_(primary_axis), secondary_axis_(secondary_axis)
  {
    BLI_assert(primary_axis_ >= 0 && primary_axis_ <= 2);
    BLI_assert(secondary_axis_ >= 0 && secondary_axis_ <= 2);
    BLI_assert(primary_axis_ != secondary_axis_);

    /* Through cancellation this will set the last axis to be the one that's neither the primary
     * nor secondary axis. */
    tertiary_axis_ = (0 + 1 + 2) - primary_axis - secondary_axis;

    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Euler from Axis", signature};
      builder.single_input<float3>("Primary");
      builder.single_input<float3>("Secondary");
      builder.single_output<float3>("Rotation");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> primaries = params.readonly_single_input<float3>(0, "Primary");
    const VArray<float3> secondaries = params.readonly_single_input<float3>(1, "Secondary");
    MutableSpan<float3> r_rotations = params.uninitialized_single_output<float3>(2, "Rotation");

    /* Might have to invert the axis to make sure that the created matrix has determinant 1. */
    const bool invert_tertiary = (secondary_axis_ + 1) % 3 == primary_axis_;
    const float tertiary_factor = invert_tertiary ? -1.0f : 1.0f;

    for (const int64_t i : mask) {
      std::cout << i << ": " << primaries[i] << "\n";
      float3 primary = math::normalize(primaries[i]);
      float3 secondary = secondaries[i];
      float3 tertiary;

      const bool primary_is_non_zero = !math::is_zero(primary);
      const bool secondary_is_non_zero = !math::is_zero(secondary);
      if (primary_is_non_zero && secondary_is_non_zero) {
        tertiary = math::cross(primary, secondary);
        if (math::is_zero(tertiary)) {
          tertiary = get_orthogonal_of_non_zero_vector(secondary);
        }
        tertiary = math::normalize(tertiary);
        secondary = math::cross(tertiary, primary);
      }
      else if (primary_is_non_zero) {
        secondary = get_orthogonal_of_non_zero_vector(primary);
        secondary = math::normalize(secondary);
        tertiary = math::cross(primary, secondary);
      }
      else if (secondary_is_non_zero) {
        secondary = math::normalize(secondary);
        primary = get_orthogonal_of_non_zero_vector(secondary);
        primary = math::normalize(primary);
        tertiary = math::cross(primary, secondary);
      }
      else {
        r_rotations[i] = {0.0f, 0.0f, 0.0f};
        continue;
      }

      float3x3 mat;
      mat[primary_axis_] = primary;
      mat[secondary_axis_] = secondary;
      mat[tertiary_axis_] = tertiary_factor * tertiary;
      BLI_assert(math::is_orthonormal(mat));
      BLI_assert(std::abs(math::determinant(mat) - 1.0f) < 0.0001f);

      const math::EulerXYZ euler = math::to_euler<float, true>(mat);
      r_rotations[i] = float3(euler);
    }
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  const NodeFunctionAxisToEuler &storage = node_storage(node);
  if (storage.legacy_distribute_node_behavior) {
    static const auto fn = mf::build::SI1_SO<float3, float3>(
        "Axis to Euler (Legacy)", [](const float3 &axis) {
          float quat[4];
          vec_to_quat(quat, axis, OB_NEGZ, OB_POSY);
          float3 rotation;
          quat_to_eul(rotation, quat);
          return rotation;
        });
    builder.set_matching_fn(fn);
    return;
  }

  if (storage.primary_axis == storage.secondary_axis) {
    return;
  }
  builder.construct_and_set_matching_fn<AxisToEulerFunction>(storage.primary_axis,
                                                             storage.secondary_axis);
}

}  // namespace blender::nodes::node_fn_axis_to_euler_cc

void register_node_type_fn_axis_to_euler()
{
  namespace file_ns = blender::nodes::node_fn_axis_to_euler_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_AXIS_TO_EULER, "Axis to Euler", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.draw_buttons_ex = file_ns::node_layout_ex;
  node_type_storage(
      &ntype, "NodeFunctionAxisToEuler", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
