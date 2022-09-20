/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ED_viewer_path.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_workspace.h"

#include "BLI_listbase.h"

#include "DNA_windowmanager_types.h"

void ED_viewer_path_activate_geometry_node(struct Main *bmain,
                                           struct SpaceNode *snode,
                                           struct bNode *node)
{
  UNUSED_VARS(bmain, snode, node);
}

bool ED_viewer_path_is_active(const struct bContext *C, const ViewerPath *viewer_path)
{
  UNUSED_VARS(C, viewer_path);
  return true;
}

bool ED_viewer_path_exists(struct Main *bmain, const ViewerPath *viewer_path)
{
  UNUSED_VARS(bmain, viewer_path);
  return true;
}

bool ED_viewer_path_tag_depsgraph(const ViewerPath *viewer_path)
{
  UNUSED_VARS(viewer_path);
  return false;
}
