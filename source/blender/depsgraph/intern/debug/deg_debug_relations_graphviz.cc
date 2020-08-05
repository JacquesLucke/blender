/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Implementation of tools for debugging the depsgraph
 */

#include <cstdarg>

#include "BLI_dot_export.hh"
#include "BLI_utildefines.h"

#include "DNA_listBase.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"

#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

namespace deg = blender::deg;
namespace dot = blender::dot;

/* ****************** */
/* Graphviz Debugging */

namespace blender {
namespace deg {

/* Only one should be enabled, defines whether graphviz nodes
 * get colored by individual types or classes.
 */
#define COLOR_SCHEME_NODE_CLASS 1
//#define COLOR_SCHEME_NODE_TYPE  2

static const char *deg_debug_graphviz_fontname = "helvetica";
static float deg_debug_graphviz_graph_label_size = 20.0f;
static float deg_debug_graphviz_node_label_size = 14.0f;
static const int deg_debug_max_colors = 12;
#ifdef COLOR_SCHEME_NODE_TYPE
static const char *deg_debug_colors[] = {
    "#a6cee3",
    "#1f78b4",
    "#b2df8a",
    "#33a02c",
    "#fb9a99",
    "#e31a1c",
    "#fdbf6f",
    "#ff7f00",
    "#cab2d6",
    "#6a3d9a",
    "#ffff99",
    "#b15928",
    "#ff00ff",
};
#endif
static const char *deg_debug_colors_light[] = {
    "#8dd3c7",
    "#ffffb3",
    "#bebada",
    "#fb8072",
    "#80b1d3",
    "#fdb462",
    "#b3de69",
    "#fccde5",
    "#d9d9d9",
    "#bc80bd",
    "#ccebc5",
    "#ffed6f",
    "#ff00ff",
};

#ifdef COLOR_SCHEME_NODE_TYPE
static const int deg_debug_node_type_color_map[][2] = {
    {NodeType::TIMESOURCE, 0},
    {NodeType::ID_REF, 1},

    /* Outer Types */
    {NodeType::PARAMETERS, 2},
    {NodeType::PROXY, 3},
    {NodeType::ANIMATION, 4},
    {NodeType::TRANSFORM, 5},
    {NodeType::GEOMETRY, 6},
    {NodeType::SEQUENCER, 7},
    {NodeType::SHADING, 8},
    {NodeType::SHADING_PARAMETERS, 9},
    {NodeType::CACHE, 10},
    {NodeType::POINT_CACHE, 11},
    {NodeType::LAYER_COLLECTIONS, 12},
    {NodeType::COPY_ON_WRITE, 13},
    {-1, 0},
};
#endif

static int deg_debug_node_color_index(const Node *node)
{
#ifdef COLOR_SCHEME_NODE_CLASS
  /* Some special types. */
  switch (node->type) {
    case NodeType::ID_REF:
      return 5;
    case NodeType::OPERATION: {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->is_noop()) {
        if (op_node->flag & OperationFlag::DEPSOP_FLAG_PINNED) {
          return 7;
        }
        return 8;
      }
      break;
    }

    default:
      break;
  }
  /* Do others based on class. */
  switch (node->get_class()) {
    case NodeClass::OPERATION:
      return 4;
    case NodeClass::COMPONENT:
      return 1;
    default:
      return 9;
  }
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
  const int(*pair)[2];
  for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; pair++) {
    if ((*pair)[0] == node->type) {
      return (*pair)[1];
    }
  }
  return -1;
#endif
}

struct DotContext {
  dot::DirectedGraph &digraph;
  Map<const Node *, dot::Node *> nodes_map;
  Map<const Node *, dot::Cluster *> clusters_map;
};

struct DebugContext {
  FILE *file;
  bool show_tags;
  DotContext *dot;
};

static void deg_debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
    ATTR_PRINTF_FORMAT(2, 3);
static void deg_debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(ctx.file, fmt, args);
  va_end(args);
}

static void deg_debug_graphviz_legend_color(const DebugContext &UNUSED(ctx),
                                            const char *name,
                                            const char *color,
                                            std::stringstream &ss)
{

  ss << "<TR>";
  ss << "<TD>" << name << "</TD>";
  ss << "<TD BGCOLOR=\"" << color << "\"></TD>";
  ss << "</TR>";
}

static void deg_debug_graphviz_legend(const DebugContext &ctx)
{
  dot::Node &legend_node = ctx.dot->digraph.new_node("");
  legend_node.set_attribute("rank", "sink");
  legend_node.set_attribute("shape", "none");
  legend_node.set_attribute("margin", 0);

  std::stringstream ss;
  ss << "<";
  ss << "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">";
  ss << "<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>";

#ifdef COLOR_SCHEME_NODE_CLASS
  const char **colors = deg_debug_colors_light;
  deg_debug_graphviz_legend_color(ctx, "Operation", colors[4], ss);
  deg_debug_graphviz_legend_color(ctx, "Component", colors[1], ss);
  deg_debug_graphviz_legend_color(ctx, "ID Node", colors[5], ss);
  deg_debug_graphviz_legend_color(ctx, "NOOP", colors[8], ss);
  deg_debug_graphviz_legend_color(ctx, "Pinned OP", colors[7], ss);
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
  const int(*pair)[2];
  for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; pair++) {
    DepsNodeFactory *nti = type_get_factory((NodeType)(*pair)[0]);
    deg_debug_graphviz_legend_color(
        ctx, nti->tname().c_str(), deg_debug_colors_light[(*pair)[1] % deg_debug_max_colors], ss);
  }
#endif

  ss << "</TABLE>";
  ss << ">";
  legend_node.set_attribute("label", ss.str());
  legend_node.set_attribute("fontname", deg_debug_graphviz_fontname);
}

static void deg_debug_graphviz_node_color(const DebugContext &ctx,
                                          const Node *node,
                                          dot::AttributeList &dot_attributes)
{
  const char *color_default = "black";
  const char *color_modified = "orangered4";
  const char *color_update = "dodgerblue3";
  const char *color = color_default;
  if (ctx.show_tags) {
    if (node->get_class() == NodeClass::OPERATION) {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->flag & DEPSOP_FLAG_DIRECTLY_MODIFIED) {
        color = color_modified;
      }
      else if (op_node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
        color = color_update;
      }
    }
  }
  dot_attributes.set("color", color);
}

static void deg_debug_graphviz_node_penwidth(const DebugContext &ctx,
                                             const Node *node,
                                             dot::AttributeList &dot_attributes)
{
  float penwidth_default = 1.0f;
  float penwidth_modified = 4.0f;
  float penwidth_update = 4.0f;
  float penwidth = penwidth_default;
  if (ctx.show_tags) {
    if (node->get_class() == NodeClass::OPERATION) {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->flag & DEPSOP_FLAG_DIRECTLY_MODIFIED) {
        penwidth = penwidth_modified;
      }
      else if (op_node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
        penwidth = penwidth_update;
      }
    }
  }
  dot_attributes.set("penwidth", penwidth);
}

static void deg_debug_graphviz_node_fillcolor(const DebugContext &UNUSED(ctx),
                                              const Node *node,
                                              dot::AttributeList &dot_attributes)
{
  const char *defaultcolor = "gainsboro";
  int color_index = deg_debug_node_color_index(node);
  const char *fillcolor = color_index < 0 ?
                              defaultcolor :
                              deg_debug_colors_light[color_index % deg_debug_max_colors];
  dot_attributes.set("fillcolor", fillcolor);
}

static void deg_debug_graphviz_relation_color(const DebugContext &UNUSED(ctx),
                                              const Relation *rel,
                                              dot::DirectedEdge &edge)
{
  const char *color_default = "black";
  const char *color_cyclic = "red4";   /* The color of crime scene. */
  const char *color_godmode = "blue4"; /* The color of beautiful sky. */
  const char *color = color_default;
  if (rel->flag & RELATION_FLAG_CYCLIC) {
    color = color_cyclic;
  }
  else if (rel->flag & RELATION_FLAG_GODMODE) {
    color = color_godmode;
  }
  edge.set_attribute("color", color);
}

static void deg_debug_graphviz_relation_style(const DebugContext &UNUSED(ctx),
                                              const Relation *rel,
                                              dot::DirectedEdge &edge)
{
  const char *style_default = "solid";
  const char *style_no_flush = "dashed";
  const char *style_flush_user_only = "dotted";
  const char *style = style_default;
  if (rel->flag & RELATION_FLAG_NO_FLUSH) {
    style = style_no_flush;
  }
  if (rel->flag & RELATION_FLAG_FLUSH_USER_EDIT_ONLY) {
    style = style_flush_user_only;
  }
  edge.set_attribute("style", style);
}

static void deg_debug_graphviz_relation_arrowhead(const DebugContext &UNUSED(ctx),
                                                  const Relation *rel,
                                                  dot::DirectedEdge &edge)
{
  const char *shape_default = "normal";
  const char *shape_no_cow = "box";
  const char *shape = shape_default;
  if (rel->from->get_class() == NodeClass::OPERATION &&
      rel->to->get_class() == NodeClass::OPERATION) {
    OperationNode *op_from = (OperationNode *)rel->from;
    OperationNode *op_to = (OperationNode *)rel->to;
    if (op_from->owner->type == NodeType::COPY_ON_WRITE &&
        !op_to->owner->need_tag_cow_before_update()) {
      shape = shape_no_cow;
    }
  }
  edge.set_attribute("arrowhead", shape);
}

static void deg_debug_graphviz_node_style(const DebugContext &ctx,
                                          const Node *node,
                                          dot::AttributeList &dot_attributes)
{
  StringRef base_style = "filled"; /* default style */
  if (ctx.show_tags) {
    if (node->get_class() == NodeClass::OPERATION) {
      OperationNode *op_node = (OperationNode *)node;
      if (op_node->flag & (DEPSOP_FLAG_DIRECTLY_MODIFIED | DEPSOP_FLAG_NEEDS_UPDATE)) {
        base_style = "striped";
      }
    }
  }
  switch (node->get_class()) {
    case NodeClass::GENERIC:
      dot_attributes.set("style", base_style);
      break;
    case NodeClass::COMPONENT:
      dot_attributes.set("style", base_style);
      break;
    case NodeClass::OPERATION:
      dot_attributes.set("style", base_style + ",rounded");
      break;
  }
}

static void deg_debug_graphviz_node_single(const DebugContext &ctx,
                                           const Node *node,
                                           dot::Cluster *parent_cluster)
{
  string name = node->identifier();

  dot::Node &dot_node = ctx.dot->digraph.new_node(name);
  ctx.dot->nodes_map.add_new(node, &dot_node);
  dot_node.set_parent_cluster(parent_cluster);
  dot_node.set_attribute("fontname", deg_debug_graphviz_fontname);
  dot_node.set_attribute("frontsize", deg_debug_graphviz_node_label_size);
  dot_node.set_attribute("shape", "box");

  deg_debug_graphviz_node_style(ctx, node, dot_node.attributes());
  deg_debug_graphviz_node_color(ctx, node, dot_node.attributes());
  deg_debug_graphviz_node_fillcolor(ctx, node, dot_node.attributes());
  deg_debug_graphviz_node_penwidth(ctx, node, dot_node.attributes());
}

static dot::Cluster &deg_debug_graphviz_node_cluster_create(const DebugContext &ctx,
                                                            const Node *node,
                                                            dot::Cluster *parent_cluster)
{
  string name = node->identifier();
  dot::Cluster &cluster = ctx.dot->digraph.new_cluster(name);
  cluster.set_parent_cluster(parent_cluster);
  cluster.set_attribute("fontname", deg_debug_graphviz_fontname);
  cluster.set_attribute("fontsize", deg_debug_graphviz_node_label_size);
  cluster.set_attribute("margin", 16);
  deg_debug_graphviz_node_style(ctx, node, cluster.attributes());
  deg_debug_graphviz_node_color(ctx, node, cluster.attributes());
  deg_debug_graphviz_node_fillcolor(ctx, node, cluster.attributes());
  deg_debug_graphviz_node_penwidth(ctx, node, cluster.attributes());
  /* dummy node, so we can add edges between clusters */
  dot::Node &dot_node = ctx.dot->digraph.new_node("");
  dot_node.set_attribute("shape", "point");
  dot_node.set_attribute("style", "invis");
  dot_node.set_parent_cluster(&cluster);
  ctx.dot->nodes_map.add_new(node, &dot_node);
  ctx.dot->clusters_map.add_new(node, &cluster);
  return cluster;
}

static void deg_debug_graphviz_graph_nodes(const DebugContext &ctx, const Depsgraph *graph);
static void deg_debug_graphviz_graph_relations(const DebugContext &ctx, const Depsgraph *graph);

static void deg_debug_graphviz_node(const DebugContext &ctx,
                                    const Node *node,
                                    dot::Cluster *parent_cluster)
{
  switch (node->type) {
    case NodeType::ID_REF: {
      const IDNode *id_node = (const IDNode *)node;
      if (id_node->components.is_empty()) {
        deg_debug_graphviz_node_single(ctx, node, parent_cluster);
      }
      else {
        dot::Cluster &cluster = deg_debug_graphviz_node_cluster_create(ctx, node, parent_cluster);
        for (const ComponentNode *comp : id_node->components.values()) {
          deg_debug_graphviz_node(ctx, comp, &cluster);
        }
      }
      break;
    }
    case NodeType::PARAMETERS:
    case NodeType::ANIMATION:
    case NodeType::TRANSFORM:
    case NodeType::PROXY:
    case NodeType::GEOMETRY:
    case NodeType::SEQUENCER:
    case NodeType::EVAL_POSE:
    case NodeType::BONE:
    case NodeType::SHADING:
    case NodeType::SHADING_PARAMETERS:
    case NodeType::CACHE:
    case NodeType::POINT_CACHE:
    case NodeType::IMAGE_ANIMATION:
    case NodeType::LAYER_COLLECTIONS:
    case NodeType::PARTICLE_SYSTEM:
    case NodeType::PARTICLE_SETTINGS:
    case NodeType::COPY_ON_WRITE:
    case NodeType::OBJECT_FROM_LAYER:
    case NodeType::BATCH_CACHE:
    case NodeType::DUPLI:
    case NodeType::SYNCHRONIZATION:
    case NodeType::AUDIO:
    case NodeType::ARMATURE:
    case NodeType::GENERIC_DATABLOCK:
    case NodeType::SIMULATION: {
      ComponentNode *comp_node = (ComponentNode *)node;
      if (comp_node->operations.is_empty()) {
        deg_debug_graphviz_node_single(ctx, node, parent_cluster);
      }
      else {
        dot::Cluster &cluster = deg_debug_graphviz_node_cluster_create(ctx, node, parent_cluster);
        for (Node *op_node : comp_node->operations) {
          deg_debug_graphviz_node(ctx, op_node, &cluster);
        }
      }
      break;
    }
    case NodeType::UNDEFINED:
    case NodeType::TIMESOURCE:
    case NodeType::OPERATION:
      deg_debug_graphviz_node_single(ctx, node, parent_cluster);
      break;
    case NodeType::NUM_TYPES:
      break;
  }
}

static void deg_debug_graphviz_node_relations(const DebugContext &ctx, const Node *node)
{
  for (Relation *rel : node->inlinks) {
    float penwidth = 2.0f;

    const Node *tail = rel->to; /* same as node */
    const Node *head = rel->from;
    dot::Node &dot_tail = *ctx.dot->nodes_map.lookup(tail);
    dot::Node &dot_head = *ctx.dot->nodes_map.lookup(head);

    dot::DirectedEdge &edge = ctx.dot->digraph.new_edge(dot_tail, dot_head);

    /* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
    edge.set_attribute("id", rel->name);
    deg_debug_graphviz_relation_color(ctx, rel, edge);
    deg_debug_graphviz_relation_style(ctx, rel, edge);
    deg_debug_graphviz_relation_arrowhead(ctx, rel, edge);
    edge.set_attribute("penwidth", penwidth);

    /* NOTE: edge from node to own cluster is not possible and gives graphviz
     * warning, avoid this here by just linking directly to the invisible
     * placeholder node. */
    dot::Cluster *tail_cluster = ctx.dot->clusters_map.lookup_default(tail, nullptr);
    if (tail_cluster != nullptr && tail_cluster->contains(dot_head)) {
      edge.set_attribute("ltail", tail_cluster->name());
    }
    dot::Cluster *head_cluster = ctx.dot->clusters_map.lookup_default(head, nullptr);
    if (head_cluster != nullptr && head_cluster->contains(dot_tail)) {
      edge.set_attribute("lhead", head_cluster->name());
    }
  }
}

static void deg_debug_graphviz_graph_nodes(const DebugContext &ctx, const Depsgraph *graph)
{
  for (Node *node : graph->id_nodes) {
    deg_debug_graphviz_node(ctx, node, nullptr);
  }
  TimeSourceNode *time_source = graph->find_time_source();
  if (time_source != nullptr) {
    deg_debug_graphviz_node(ctx, time_source, nullptr);
  }
}

static void deg_debug_graphviz_graph_relations(const DebugContext &ctx, const Depsgraph *graph)
{
  for (IDNode *id_node : graph->id_nodes) {
    for (ComponentNode *comp_node : id_node->components.values()) {
      for (OperationNode *op_node : comp_node->operations) {
        deg_debug_graphviz_node_relations(ctx, op_node);
      }
    }
  }

  TimeSourceNode *time_source = graph->find_time_source();
  if (time_source != nullptr) {
    deg_debug_graphviz_node_relations(ctx, time_source);
  }
}

}  // namespace deg
}  // namespace blender

void DEG_debug_relations_graphviz(const Depsgraph *graph, FILE *f, const char *label)
{
  if (!graph) {
    return;
  }

  const deg::Depsgraph *deg_graph = reinterpret_cast<const deg::Depsgraph *>(graph);

  dot::DirectedGraph digraph;

  deg::DotContext dot_context = {digraph};

  deg::DebugContext ctx;
  ctx.file = f;
  ctx.dot = &dot_context;
  ctx.show_tags = false;

  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);
  digraph.set_attribute("compound", "true");
  digraph.set_attribute("labelloc", "t");
  digraph.set_attribute("fontsize", deg::deg_debug_graphviz_graph_label_size);
  digraph.set_attribute("fontname", deg::deg_debug_graphviz_fontname);
  digraph.set_attribute("label", label);
  digraph.set_attribute("splines", "ortho");
  digraph.set_attribute("overlap", "scalexy");

  deg::deg_debug_graphviz_graph_nodes(ctx, deg_graph);
  deg::deg_debug_graphviz_graph_relations(ctx, deg_graph);

  deg::deg_debug_graphviz_legend(ctx);

  std::string dot_string = digraph.to_dot_string();
  deg_debug_fprintf(ctx, dot_string.c_str());
}
