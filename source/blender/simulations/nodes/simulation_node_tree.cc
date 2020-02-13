#include <cstring>

#include "SIM_node_tree.h"
#include "BKE_node.h"
#include "MEM_guardedalloc.h"
#include "BLT_translation.h"
#include "RNA_access.h"

bNodeTreeType *ntreeType_Simulation;

void register_node_tree_type_sim()
{
  bNodeTreeType *tt = ntreeType_Simulation = (bNodeTreeType *)MEM_callocN(
      sizeof(bNodeTreeType), "simulation node tree type");
  tt->type = NTREE_SIMULATION;
  strcpy(tt->idname, "SimulationNodeTree");
  strcpy(tt->ui_name, N_("Simulation Editor"));
  strcpy(tt->ui_description, N_("Simulation nodes"));
  tt->ui_icon = 0; /* Defined in drawnode.c */
  tt->ext.srna = &RNA_SimulationNodeTree;

  ntreeTypeAdd(tt);
}
