/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_viewer_path.h"

struct Main;
struct SpaceNode;
struct bNode;
struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

void ED_viewer_path_activate_geometry_node(struct Main *bmain,
                                           struct SpaceNode *snode,
                                           struct bNode *node);
bool ED_viewer_path_is_active(const struct bContext *C, const ViewerPath *viewer_path);
bool ED_viewer_path_exists(struct Main *bmain, const ViewerPath *viewer_path);
bool ED_viewer_path_tag_depsgraph(const ViewerPath *viewer_path);

#ifdef __cplusplus
}
#endif
