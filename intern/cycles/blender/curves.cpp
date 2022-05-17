/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include <optional>

#include "blender/sync.h"
#include "blender/util.h"

#include "scene/attribute.h"
#include "scene/camera.h"
#include "scene/curves.h"
#include "scene/hair.h"
#include "scene/object.h"
#include "scene/scene.h"

#include "util/color.h"
#include "util/foreach.h"
#include "util/hash.h"
#include "util/log.h"

CCL_NAMESPACE_BEGIN

/* curve functions */

static void export_hair_motion_validate_attribute(Hair *hair,
                                                  int motion_step,
                                                  int num_motion_keys,
                                                  bool have_motion)
{
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  const int num_keys = hair->get_curve_keys().size();

  if (num_motion_keys != num_keys || !have_motion) {
    /* No motion or hair "topology" changed, remove attributes again. */
    if (num_motion_keys != num_keys) {
      VLOG(1) << "Hair topology changed, removing motion attribute.";
    }
    hair->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
  }
  else if (motion_step > 0) {
    /* Motion, fill up previous steps that we might have skipped because
     * they had no motion, but we need them anyway now. */
    for (int step = 0; step < motion_step; step++) {
      float4 *mP = attr_mP->data_float4() + step * num_keys;

      for (int key = 0; key < num_keys; key++) {
        mP[key] = float3_to_float4(hair->get_curve_keys()[key]);
        mP[key].w = hair->get_curve_radius()[key];
      }
    }
  }
}

#ifdef WITH_NEW_CURVES_TYPE

static std::optional<BL::FloatAttribute> find_curves_radius_attribute(BL::Curves b_curves)
{
  for (BL::Attribute &b_attribute : b_curves.attributes) {
    if (b_attribute.name() != "radius") {
      continue;
    }
    if (b_attribute.domain() != BL::Attribute::domain_POINT) {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_FLOAT) {
      continue;
    }
    return BL::FloatAttribute{b_attribute};
  }
  return std::nullopt;
}

template<typename TypeInCycles, typename GetValueAtIndex>
static void fill_generic_attribute(BL::Curves &b_curves,
                                   TypeInCycles *data,
                                   const AttributeElement element,
                                   const GetValueAtIndex &get_value_at_index)
{
  switch (element) {
    case ATTR_ELEMENT_CURVE_KEY: {
      const int num_points = b_curves.points.length();
      for (int i = 0; i < num_points; i++) {
        data[i] = get_value_at_index(i);
      }
      break;
    }
    case ATTR_ELEMENT_CURVE: {
      const int num_verts = b_curves.curves.length();
      for (int i = 0; i < num_verts; i++) {
        data[i] = get_value_at_index(i);
      }
      break;
    }
    default: {
      assert(false);
      break;
    }
  }
}

static void attr_create_motion(Hair *hair, BL::Attribute &b_attribute, const float motion_scale)
{
  if (!(b_attribute.domain() == BL::Attribute::domain_POINT) &&
      (b_attribute.data_type() == BL::Attribute::data_type_FLOAT_VECTOR)) {
    return;
  }

  BL::FloatVectorAttribute b_vector_attribute(b_attribute);
  const int num_curve_keys = hair->get_curve_keys().size();

  /* Find or add attribute */
  float3 *P = &hair->get_curve_keys()[0];
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (!attr_mP) {
    attr_mP = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  /* Only export previous and next frame, we don't have any in between data. */
  float motion_times[2] = {-1.0f, 1.0f};
  for (int step = 0; step < 2; step++) {
    const float relative_time = motion_times[step] * 0.5f * motion_scale;
    float3 *mP = attr_mP->data_float3() + step * num_curve_keys;

    for (int i = 0; i < num_curve_keys; i++) {
      mP[i] = P[i] + get_float3(b_vector_attribute.data[i].vector()) * relative_time;
    }
  }
}

static void attr_create_uv(AttributeSet &attributes,
                           BL::Curves &b_curves,
                           BL::Attribute &b_attribute,
                           const ustring name)
{
  BL::Float2Attribute b_float2_attribute{b_attribute};
  Attribute *attr = attributes.add(ATTR_STD_UV, name);

  float2 *data = attr->data_float2();
  fill_generic_attribute(b_curves, data, ATTR_ELEMENT_CURVE, [&](int i) {
    BL::Array<float, 2> v = b_float2_attribute.data[i].vector();
    return make_float2(v[0], v[1]);
  });
}

static void attr_create_generic(Scene *scene,
                                Hair *hair,
                                BL::Curves &b_curves,
                                const bool need_motion,
                                const float motion_scale)
{
  AttributeSet &attributes = hair->attributes;
  static const ustring u_velocity("velocity");
  const bool need_uv = hair->need_attribute(scene, ATTR_STD_UV);
  bool have_uv = false;

  for (BL::Attribute &b_attribute : b_curves.attributes) {
    const ustring name{b_attribute.name().c_str()};

    const BL::Attribute::domain_enum b_domain = b_attribute.domain();
    const BL::Attribute::data_type_enum b_data_type = b_attribute.data_type();

    if (need_motion && name == u_velocity) {
      attr_create_motion(hair, b_attribute, motion_scale);
      continue;
    }

    /* Weak, use first float2 attribute as standard UV. */
    if (need_uv && !have_uv && b_data_type == BL::Attribute::data_type_FLOAT2 &&
        b_domain == BL::Attribute::domain_CURVE) {
      attr_create_uv(attributes, b_curves, b_attribute, name);
      have_uv = true;
      continue;
    }

    if (!hair->need_attribute(scene, name)) {
      continue;
    }
    if (attributes.find(name)) {
      continue;
    }

    AttributeElement element = ATTR_ELEMENT_NONE;
    switch (b_domain) {
      case BL::Attribute::domain_POINT:
        element = ATTR_ELEMENT_CURVE_KEY;
        break;
      case BL::Attribute::domain_CURVE:
        element = ATTR_ELEMENT_CURVE;
        break;
      default:
        break;
    }
    if (element == ATTR_ELEMENT_NONE) {
      /* Not supported. */
      continue;
    }
    switch (b_data_type) {
      case BL::Attribute::data_type_FLOAT: {
        BL::FloatAttribute b_float_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_curves, data, element, [&](int i) { return b_float_attribute.data[i].value(); });
        break;
      }
      case BL::Attribute::data_type_BOOLEAN: {
        BL::BoolAttribute b_bool_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(b_curves, data, element, [&](int i) {
          return (float)b_bool_attribute.data[i].value();
        });
        break;
      }
      case BL::Attribute::data_type_INT: {
        BL::IntAttribute b_int_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(b_curves, data, element, [&](int i) {
          return (float)b_int_attribute.data[i].value();
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT_VECTOR: {
        BL::FloatVectorAttribute b_vector_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeVector, element);
        float3 *data = attr->data_float3();
        fill_generic_attribute(b_curves, data, element, [&](int i) {
          BL::Array<float, 3> v = b_vector_attribute.data[i].vector();
          return make_float3(v[0], v[1], v[2]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT_COLOR: {
        BL::FloatColorAttribute b_color_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        float4 *data = attr->data_float4();
        fill_generic_attribute(b_curves, data, element, [&](int i) {
          BL::Array<float, 4> v = b_color_attribute.data[i].color();
          return make_float4(v[0], v[1], v[2], v[3]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT2: {
        BL::Float2Attribute b_float2_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        fill_generic_attribute(b_curves, data, element, [&](int i) {
          BL::Array<float, 2> v = b_float2_attribute.data[i].vector();
          return make_float2(v[0], v[1]);
        });
        break;
      }
      default:
        /* Not supported. */
        break;
    }
  }
}

static float4 hair_point_as_float4(BL::Curves b_curves,
                                   std::optional<BL::FloatAttribute> b_attr_radius,
                                   const int index)
{
  float4 mP = float3_to_float4(get_float3(b_curves.position_data[index].vector()));
  mP.w = b_attr_radius ? b_attr_radius->data[index].value() : 0.0f;
  return mP;
}

static float4 interpolate_hair_points(BL::Curves b_curves,
                                      std::optional<BL::FloatAttribute> b_attr_radius,
                                      const int first_point_index,
                                      const int num_points,
                                      const float step)
{
  const float curve_t = step * (num_points - 1);
  const int point_a = clamp((int)curve_t, 0, num_points - 1);
  const int point_b = min(point_a + 1, num_points - 1);
  const float t = curve_t - (float)point_a;
  return lerp(hair_point_as_float4(b_curves, b_attr_radius, first_point_index + point_a),
              hair_point_as_float4(b_curves, b_attr_radius, first_point_index + point_b),
              t);
}

static void export_hair_curves(Scene *scene,
                               Hair *hair,
                               BL::Curves b_curves,
                               const bool need_motion,
                               const float motion_scale)
{
  /* TODO: optimize so we can straight memcpy arrays from Blender? */

  /* Reserve memory. */
  const int num_keys = b_curves.points.length();
  const int num_curves = b_curves.curves.length();

  hair->resize_curves(num_curves, num_keys);

  float3 *curve_keys = hair->get_curve_keys().data();
  float *curve_radius = hair->get_curve_radius().data();
  int *curve_first_key = hair->get_curve_first_key().data();
  int *curve_shader = hair->get_curve_shader().data();

  /* Add requested attributes. */
  float *attr_intercept = NULL;
  float *attr_length = NULL;
  float *attr_random = NULL;

  if (hair->need_attribute(scene, ATTR_STD_CURVE_INTERCEPT)) {
    attr_intercept = hair->attributes.add(ATTR_STD_CURVE_INTERCEPT)->data_float();
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_LENGTH)) {
    attr_length = hair->attributes.add(ATTR_STD_CURVE_LENGTH)->data_float();
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_RANDOM)) {
    attr_random = hair->attributes.add(ATTR_STD_CURVE_RANDOM)->data_float();
  }

  std::optional<BL::FloatAttribute> b_attr_radius = find_curves_radius_attribute(b_curves);

  /* Export curves and points. */
  for (int i = 0; i < num_curves; i++) {
    const int first_point_index = b_curves.curve_offset_data[i].value();
    const int num_points = b_curves.curve_offset_data[i + 1].value() - first_point_index;

    float3 prev_co = zero_float3();
    float length = 0.0f;

    /* Position and radius. */
    for (int j = 0; j < num_points; j++) {
      const int point_offset = first_point_index + j;
      const float3 co = get_float3(b_curves.position_data[point_offset].vector());
      const float radius = b_attr_radius ? b_attr_radius->data[point_offset].value() : 0.0f;

      curve_keys[point_offset] = co;
      curve_radius[point_offset] = radius;

      if (attr_length || attr_intercept) {
        if (j > 0) {
          length += len(co - prev_co);
        }
        prev_co = co;

        if (attr_intercept) {
          attr_intercept[point_offset] = length;
        }
      }
    }

    /* Normalized 0..1 attribute along curve. */
    if (attr_intercept && length > 0.0f) {
      for (int j = 1; j < num_points; j++) {
        const int point_offset = first_point_index + j;
        attr_intercept[point_offset] /= length;
      }
    }

    /* Curve length. */
    if (attr_length) {
      attr_length[i] = length;
    }

    /* Random number per curve. */
    if (attr_random != NULL) {
      attr_random[i] = hash_uint2_to_float(i, 0);
    }

    /* Curve. */
    curve_shader[i] = 0;
    curve_first_key[i] = first_point_index;
  }

  attr_create_generic(scene, hair, b_curves, need_motion, motion_scale);
}

static void export_hair_curves_motion(Hair *hair, BL::Curves b_curves, int motion_step)
{
  /* Find or add attribute. */
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  bool new_attribute = false;

  if (!attr_mP) {
    attr_mP = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    new_attribute = true;
  }

  /* Export motion keys. */
  const int num_keys = hair->get_curve_keys().size();
  const int num_curves = b_curves.curves.length();
  float4 *mP = attr_mP->data_float4() + motion_step * num_keys;
  bool have_motion = false;
  int num_motion_keys = 0;
  int curve_index = 0;

  std::optional<BL::FloatAttribute> b_attr_radius = find_curves_radius_attribute(b_curves);

  for (int i = 0; i < num_curves; i++) {
    const int first_point_index = b_curves.curve_offset_data[i].value();
    const int num_points = b_curves.curve_offset_data[i + 1].value() - first_point_index;

    Hair::Curve curve = hair->get_curve(curve_index);
    curve_index++;

    if (num_points == curve.num_keys) {
      /* Number of keys matches. */
      for (int i = 0; i < num_points; i++) {
        int point_index = first_point_index + i;

        if (point_index < num_keys) {
          mP[num_motion_keys] = hair_point_as_float4(b_curves, b_attr_radius, point_index);
          num_motion_keys++;

          if (!have_motion) {
            /* TODO: use epsilon for comparison? Was needed for particles due to
             * transform, but ideally should not happen anymore. */
            float4 curve_key = float3_to_float4(hair->get_curve_keys()[i]);
            curve_key.w = hair->get_curve_radius()[i];
            have_motion = !(mP[i] == curve_key);
          }
        }
      }
    }
    else {
      /* Number of keys has changed. Generate an interpolated version
       * to preserve motion blur. */
      const float step_size = curve.num_keys > 1 ? 1.0f / (curve.num_keys - 1) : 0.0f;
      for (int i = 0; i < curve.num_keys; i++) {
        const float step = i * step_size;
        mP[num_motion_keys] = interpolate_hair_points(
            b_curves, b_attr_radius, first_point_index, num_points, step);
        num_motion_keys++;
      }
      have_motion = true;
    }
  }

  /* In case of new attribute, we verify if there really was any motion. */
  if (new_attribute) {
    export_hair_motion_validate_attribute(hair, motion_step, num_motion_keys, have_motion);
  }
}

/* Hair object. */
void BlenderSync::sync_hair(Hair *hair, BObjectInfo &b_ob_info, bool motion, int motion_step)
{
  /* Motion blur attribute is relative to seconds, we need it relative to frames. */
  const bool need_motion = object_need_motion_attribute(b_ob_info, scene);
  const float motion_scale = (need_motion) ?
                                 scene->motion_shutter_time() /
                                     (b_scene.render().fps() / b_scene.render().fps_base()) :
                                 0.0f;

  /* Convert Blender hair to Cycles curves. */
  BL::Curves b_curves(b_ob_info.object_data);
  if (motion) {
    export_hair_curves_motion(hair, b_curves, motion_step);
  }
  else {
    export_hair_curves(scene, hair, b_curves, need_motion, motion_scale);
  }
}
#else
void BlenderSync::sync_hair(Hair *hair, BObjectInfo &b_ob_info, bool motion, int motion_step)
{
  (void)hair;
  (void)b_ob_info;
  (void)motion;
  (void)motion_step;
}
#endif

void BlenderSync::sync_hair(BObjectInfo &b_ob_info, Hair *hair)
{
  /* make a copy of the shaders as the caller in the main thread still need them for syncing the
   * attributes */
  array<Node *> used_shaders = hair->get_used_shaders();

  Hair new_hair;
  new_hair.set_used_shaders(used_shaders);

  if (view_layer.use_hair) {
#ifdef WITH_NEW_CURVES_TYPE
    if (b_ob_info.object_data.is_a(&RNA_Curves)) {
      /* Hair object. */
      sync_hair(&new_hair, b_ob_info, false);
    }
#endif
  }

  /* update original sockets */

  for (const SocketType &socket : new_hair.type->inputs) {
    /* Those sockets are updated in sync_object, so do not modify them. */
    if (socket.name == "use_motion_blur" || socket.name == "motion_steps" ||
        socket.name == "used_shaders") {
      continue;
    }
    hair->set_value(socket, new_hair, socket);
  }

  hair->attributes.update(std::move(new_hair.attributes));

  /* tag update */

  /* Compares curve_keys rather than strands in order to handle quick hair
   * adjustments in dynamic BVH - other methods could probably do this better. */
  const bool rebuild = (hair->curve_keys_is_modified() || hair->curve_radius_is_modified());

  hair->tag_update(scene, rebuild);
}

void BlenderSync::sync_hair_motion(BObjectInfo &b_ob_info, Hair *hair, int motion_step)
{
  /* Skip if nothing exported. */
  if (hair->num_keys() == 0) {
    return;
  }

  /* Export deformed coordinates. */
  if (ccl::BKE_object_is_deform_modified(b_ob_info, b_scene, preview)) {
#ifdef WITH_NEW_CURVES_TYPE
    if (b_ob_info.object_data.is_a(&RNA_Curves)) {
      /* Hair object. */
      sync_hair(hair, b_ob_info, true, motion_step);
      return;
    }
#endif
  }

  /* No deformation on this frame, copy coordinates if other frames did have it. */
  hair->copy_center_to_motion_step(motion_step);
}

CCL_NAMESPACE_END
