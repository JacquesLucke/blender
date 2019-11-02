#pragma once

#include "FN_data_flow_nodes.hpp"

#include "particle_function.hpp"

namespace BParticles {

using BKE::VNode;
using FN::DataSocket;
using FN::DataFlowNodes::VTreeDataGraph;

Vector<DataSocket> find_input_data_sockets(const VNode &vnode, VTreeDataGraph &data_graph);

Optional<std::unique_ptr<ParticleFunction>> create_particle_function(const VNode &vnode,
                                                                     VTreeDataGraph &data_graph);

}  // namespace BParticles
