/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

void register_node_type_geo_simulation()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SIMULATION, "Simulation", NODE_CLASS_LAYOUT);
  nodeRegisterType(&ntype);
}
