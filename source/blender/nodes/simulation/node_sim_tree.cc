#include <string.h>

#include "MEM_guardedalloc.h"

#include "NOD_simulation.h"

#include "BKE_node.h"

#include "BLT_translation.h"

#include "DNA_node_types.h"

#include "RNA_access.h"

bNodeTreeType *ntreeType_Simulation;

void register_node_tree_type_sim(void)
{
  bNodeTreeType *tt = ntreeType_Simulation = (bNodeTreeType *)MEM_callocN(
      sizeof(bNodeTreeType), "simulation node tree type");
  tt->type = NTREE_SIMULATION;
  strcpy(tt->idname, "SimulationNodeTree");
  strcpy(tt->ui_name, N_("Simulation Editor"));
  tt->ui_icon = 0; /* defined in drawnode.c */
  strcpy(tt->ui_description, N_("Simulation nodes"));
  tt->poll = [](const bContext *UNUSED(C), bNodeTreeType *UNUSED(treetype)) { return true; };
  tt->ext.srna = &RNA_SimulationNodeTree;

  ntreeTypeAdd(tt);
}