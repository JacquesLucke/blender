#include "BLI_timeit.hpp"
#include "BLI_multi_map.hpp"

#include "node_frontend.hpp"
#include "inserters.hpp"
#include "integrator.hpp"
#include "particle_function_builder.hpp"

namespace BParticles {

using BLI::MultiMap;
using BLI::ValueOrError;

static ArrayRef<VirtualNode *> get_type_nodes(VirtualNodeTree &vtree)
{
  return vtree.nodes_with_idname("bp_ParticleTypeNode");
}

std::unique_ptr<StepDescription> step_description_from_node_tree(VirtualNodeTree &vtree,
                                                                 WorldState &world_state,
                                                                 float time_step)
{
  SCOPED_TIMER(__func__);

  Set<std::string> particle_type_names;
  StringMap<AttributesDeclaration> declarations;

  for (VirtualNode *particle_type_node : get_type_nodes(vtree)) {
    AttributesDeclaration attributes;
    attributes.add<float3>("Position", {0, 0, 0});
    attributes.add<float3>("Velocity", {0, 0, 0});
    attributes.add<float>("Size", 0.01f);
    attributes.add<rgba_f>("Color", {1.0f, 1.0f, 1.0f, 1.0f});
    declarations.add_new(particle_type_node->name(), attributes);
    particle_type_names.add_new(particle_type_node->name());
  }

  ValueOrError<VTreeDataGraph> data_graph_or_error = FN::DataFlowNodes::generate_graph(vtree);
  if (data_graph_or_error.is_error()) {
    return {};
  }

  VTreeDataGraph data_graph = data_graph_or_error.extract_value();

  BuildContext ctx = {data_graph, particle_type_names, world_state};

  Components components;
  for (auto item : get_component_loaders().items()) {
    for (VirtualNode *vnode : vtree.nodes_with_idname(item.key)) {
      item.value(ctx, components, vnode);
    }
  }

  StringMap<ParticleType *> particle_types;
  for (VirtualNode *vnode : get_type_nodes(vtree)) {
    std::string name = vnode->name();
    ArrayRef<Force *> forces_on_type = components.m_forces.lookup_default(name);

    Integrator *integrator = nullptr;
    if (forces_on_type.size() == 0) {
      integrator = new ConstantVelocityIntegrator();
    }
    else {
      integrator = new EulerIntegrator(forces_on_type);
    }

    ParticleType *particle_type = new ParticleType(
        declarations.lookup(name),
        integrator,
        components.m_events.lookup_default(name),
        components.m_offset_handlers.lookup_default(name));
    particle_types.add_new(name, particle_type);
  }

  StepDescription *step_description = new StepDescription(
      time_step, particle_types, components.m_emitters);
  return std::unique_ptr<StepDescription>(step_description);
}

}  // namespace BParticles
