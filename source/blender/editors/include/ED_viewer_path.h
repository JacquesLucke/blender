/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_viewer_path.h"

struct Main;
struct SpaceNode;
struct bNode;
struct bContext;
struct Object;

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

#ifdef __cplusplus

#  include <optional>

#  include "BLI_string_ref.hh"
#  include "BLI_vector.hh"

struct ViewerPathForGeometryNodesViewer {
  Object *object;
  blender::StringRefNull modifier_name;
  blender::Vector<blender::StringRefNull> group_node_names;
  blender::StringRefNull viewer_node_name;
};

std::optional<ViewerPathForGeometryNodesViewer> ED_viewer_path_parse_geometry_nodes_viewer(
    const ViewerPath &viewer_path);

#endif
