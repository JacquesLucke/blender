#pragma once

#include "BLI_value_or_error.hpp"
#include "FN_data_flow_nodes.hpp"

#include "particle_function.hpp"

namespace BParticles {

using BKE::VirtualNode;
using BLI::ValueOrError;
using FN::DFGraphSocket;
using FN::DataFlowNodes::VTreeDataGraph;

Vector<DFGraphSocket> find_input_data_sockets(VirtualNode *vnode, VTreeDataGraph &data_graph);

ValueOrError<ParticleFunction> create_particle_function(VirtualNode *vnode,
                                                        VTreeDataGraph &data_graph);

}  // namespace BParticles
