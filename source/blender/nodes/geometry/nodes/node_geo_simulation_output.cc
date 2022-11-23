/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_simulation_output_cc {

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

  const GeoNodesLFUserData &lf_data = *params.user_data();
  bke::ComputeCaches &all_caches = *lf_data.modifier_data->cache_per_frame;
  bke::CacheData &cache = all_caches.cache_per_context.lookup_or_add_default(
      lf_data.compute_context->hash());

  if (cache.geometry_per_frame.contains(scene_frame)) {
    params.set_output("Geometry", cache.geometry_per_frame.lookup(scene_frame));
    // params.set_input_unused("Geometry");
    return;
  }

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set.ensure_owns_direct_data();
  cache.geometry_per_frame.add_new(scene_frame, geometry_set);

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_simulation_output_cc

void register_node_type_geo_simulation_output()
{
  namespace file_ns = blender::nodes::node_geo_simulation_output_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SIMULATION_OUTPUT, "Simulation Output", NODE_CLASS_INTERFACE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
