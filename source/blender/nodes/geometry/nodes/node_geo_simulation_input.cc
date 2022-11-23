/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_simulation_input_cc {

NODE_STORAGE_FUNCS(NodeGeometrySimulationInput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>(N_("Run"));
  b.add_input<decl::Geometry>(N_("Geometry"));

  b.add_output<decl::Float>(N_("Delta Time"));
  b.add_output<decl::Float>(N_("Elapsed Time"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationInput *data = MEM_cnew<NodeGeometrySimulationInput>(__func__);
  data->dummy = false;
  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometrySimulationInput &storage = node_storage(params.node());
  const Scene *scene = DEG_get_input_scene(params.depsgraph());
  const float scene_ctime = BKE_scene_ctime_get(scene);
  const int scene_frame = int(scene_ctime);

  /* TODO: Somehow use "Run" input. */

  const GeoNodesLFUserData &lf_data = *params.user_data();
  bke::ComputeCaches &all_caches = *lf_data.modifier_data->cache_per_frame;
  const bke::SimulationCache *cache = all_caches.lookup_context(lf_data.compute_context->hash());
  if (!cache) {
    params.set_output("Geometry", params.extract_input<GeometrySet>("Geometry"));
    return;
  }

  if (const bke::GeometryCacheValue *data = cache->value_before_time(scene_frame)) {
    if (params.lazy_output_is_required("Geometry")) {
      params.set_output("Geometry", std::move(data->geometry_set));
    }
    if (params.lazy_output_is_required("Delta Time")) {
      params.set_output("Delta Time", scene_ctime - data->time);
    }
    if (params.lazy_output_is_required("Elapsed Time")) {
      params.set_output("Delta Time", scene_ctime - cache->geometry_per_frame.first().time);
    }
    return;
  }

  if (params.lazy_require_input("Geometry")) {
    return;
  }

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  if (params.lazy_output_is_required("Delta Time")) {
    params.set_output("Delta Time", -1.0f); /* TODO: How to get this?*/
  }
  if (params.lazy_output_is_required("Elapsed Time")) {
    if (cache->geometry_per_frame.is_empty()) {
      params.set_output("Elapsed Time", 0.0f);
    }
    else {
      params.set_output("Elapsed Time", scene_ctime - cache->geometry_per_frame.first().time);
    }
  }
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_simulation_input_cc

void register_node_type_geo_simulation_input()
{
  namespace file_ns = blender::nodes::node_geo_simulation_input_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_SIMULATION_INPUT, "Simulation Input", NODE_CLASS_INTERFACE);
  ntype.initfunc = file_ns::node_init;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  node_type_storage(&ntype,
                    "NodeGeometrySimulationInput",
                    node_free_standard_storage,
                    node_copy_standard_storage);

  ntype.geometry_node_execute_supports_laziness = true;
  // ntype.declaration_is_dynamic = true;
  nodeRegisterType(&ntype);
}
