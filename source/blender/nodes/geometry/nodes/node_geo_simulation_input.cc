/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_simulation_input_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const Scene *scene = DEG_get_input_scene(params.depsgraph());
  const float scene_ctime = BKE_scene_ctime_get(scene);
  const int scene_frame = int(scene_ctime);
  const int previous_frame = scene_frame - 1;

  const GeoNodesLFUserData &lf_data = *params.user_data();
  bke::ComputeCaches &all_caches = *lf_data.modifier_data->cache_per_frame;
  bke::CacheData *cache = all_caches.cache_per_context.lookup_ptr(lf_data.compute_context->hash());
  if (!cache) {
    params.set_output("Geometry", params.extract_input<GeometrySet>("Geometry"));
    return;
  }

  if (cache->geometry_per_frame.contains(previous_frame)) {
    GeometrySet geometry_set = cache->geometry_per_frame.lookup(previous_frame);
    params.set_output("Geometry", std::move(geometry_set));
    // params.set_input_unused("Geometry");
    return;
  }

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_simulation_input_cc

void register_node_type_geo_simulation_input()
{
  namespace file_ns = blender::nodes::node_geo_simulation_input_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SIMULATION_INPUT, "Simulation Input", NODE_CLASS_INTERFACE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
