# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

import os
import bpy
import json
import nodeitems_utils
from bpy.types import (
    Operator,
    PropertyGroup,
)
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    EnumProperty,
    IntProperty,
    StringProperty,
)


class NodeSetting(PropertyGroup):
    value: StringProperty(
        name="Value",
        description="Python expression to be evaluated "
        "as the initial node setting",
        default="",
    )


# Base class for node 'Add' operators
class NodeAddOperator:

    type: StringProperty(
        name="Node Type",
        description="Node type",
    )
    use_transform: BoolProperty(
        name="Use Transform",
        description="Start transform operator after inserting the node",
        default=False,
    )
    settings: CollectionProperty(
        name="Settings",
        description="Settings to be applied on the newly created node",
        type=NodeSetting,
        options={'SKIP_SAVE'},
    )

    @staticmethod
    def store_mouse_cursor(context, event):
        space = context.space_data
        tree = space.edit_tree

        # convert mouse position to the View2D for later node placement
        if context.region.type == 'WINDOW':
            # convert mouse position to the View2D for later node placement
            space.cursor_location_from_region(
                event.mouse_region_x, event.mouse_region_y)
        else:
            space.cursor_location = tree.view_center

    # XXX explicit node_type argument is usually not necessary,
    # but required to make search operator work:
    # add_search has to override the 'type' property
    # since it's hardcoded in bpy_operator_wrap.c ...
    def create_node(self, context, node_type=None):
        space = context.space_data
        tree = space.edit_tree

        if node_type is None:
            node_type = self.type

        # select only the new node
        for n in tree.nodes:
            n.select = False

        node = tree.nodes.new(type=node_type)

        for setting in self.settings:
            # XXX catch exceptions here?
            value = eval(setting.value)

            try:
                setattr(node, setting.name, value)
            except AttributeError as e:
                self.report(
                    {'ERROR_INVALID_INPUT'},
                    "Node has no attribute " + setting.name)
                print(str(e))
                # Continue despite invalid attribute

        node.select = True
        tree.nodes.active = node
        node.location = space.cursor_location
        return node

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree to add nodes to
        return ((space.type == 'NODE_EDITOR') and
                space.edit_tree and not space.edit_tree.library)

    # Default execute simply adds a node
    def execute(self, context):
        if self.properties.is_property_set("type"):
            self.create_node(context)
            return {'FINISHED'}
        else:
            return {'CANCELLED'}

    # Default invoke stores the mouse position to place the node correctly
    # and optionally invokes the transform operator
    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        result = self.execute(context)

        if self.use_transform and ('FINISHED' in result):
            # removes the node again if transform is canceled
            bpy.ops.node.translate_attach_remove_on_cancel('INVOKE_DEFAULT')

        return result


# Simple basic operator for adding a node
class NODE_OT_add_node(NodeAddOperator, Operator):
    '''Add a node to the active tree'''
    bl_idname = "node.add_node"
    bl_label = "Add Node"
    bl_options = {'REGISTER', 'UNDO'}


# Add a node and link it to an existing socket
class NODE_OT_add_and_link_node(NodeAddOperator, Operator):
    '''Add a node to the active tree and link to an existing socket'''
    bl_idname = "node.add_and_link_node"
    bl_label = "Add and Link Node"
    bl_options = {'REGISTER', 'UNDO'}

    link_socket_index: IntProperty(
        name="Link Socket Index",
        description="Index of the socket to link",
    )

    def execute(self, context):
        space = context.space_data
        ntree = space.edit_tree

        node = self.create_node(context)
        if not node:
            return {'CANCELLED'}

        to_socket = getattr(context, "link_to_socket", None)
        if to_socket:
            ntree.links.new(node.outputs[self.link_socket_index], to_socket)

        from_socket = getattr(context, "link_from_socket", None)
        if from_socket:
            ntree.links.new(from_socket, node.inputs[self.link_socket_index])

        return {'FINISHED'}


class NODE_OT_add_search(NodeAddOperator, Operator):
    '''Add a node to the active tree'''
    bl_idname = "node.add_search"
    bl_label = "Search and Add Node"
    bl_options = {'REGISTER', 'UNDO'}
    bl_property = "node_item"

    _enum_item_hack = []

    # Create an enum list from node items
    def node_enum_items(self, context):
        enum_items = NODE_OT_add_search._enum_item_hack
        enum_items.clear()

        for index, item in enumerate(nodeitems_utils.node_items_iter(context)):
            if isinstance(item, nodeitems_utils.NodeItem):
                enum_items.append(
                    (str(index),
                     item.label,
                     "",
                     index,
                     ))
        return enum_items

    # Look up the item based on index
    def find_node_item(self, context):
        node_item = int(self.node_item)
        for index, item in enumerate(nodeitems_utils.node_items_iter(context)):
            if index == node_item:
                return item
        return None

    node_item: EnumProperty(
        name="Node Type",
        description="Node type",
        items=node_enum_items,
    )

    def execute(self, context):
        item = self.find_node_item(context)

        # no need to keep
        self._enum_item_hack.clear()

        if item:
            # apply settings from the node item
            for setting in item.settings.items():
                ops = self.settings.add()
                ops.name = setting[0]
                ops.value = setting[1]

            self.create_node(context, item.nodetype)

            if self.use_transform:
                bpy.ops.node.translate_attach_remove_on_cancel(
                    'INVOKE_DEFAULT')

            return {'FINISHED'}
        else:
            return {'CANCELLED'}

    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        # Delayed execution in the search popup
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}


class NODE_OT_collapse_hide_unused_toggle(Operator):
    '''Toggle collapsed nodes and hide unused sockets'''
    bl_idname = "node.collapse_hide_unused_toggle"
    bl_label = "Collapse and Hide Unused Sockets"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree
        return ((space.type == 'NODE_EDITOR') and
                (space.edit_tree and not space.edit_tree.library))

    def execute(self, context):
        space = context.space_data
        tree = space.edit_tree

        for node in tree.nodes:
            if node.select:
                hide = (not node.hide)

                node.hide = hide
                # Note: connected sockets are ignored internally
                for socket in node.inputs:
                    socket.hide = hide
                for socket in node.outputs:
                    socket.hide = hide

        return {'FINISHED'}


class NODE_OT_tree_path_parent(Operator):
    '''Go to parent node tree'''
    bl_idname = "node.tree_path_parent"
    bl_label = "Parent Node Tree"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree
        return (space.type == 'NODE_EDITOR' and len(space.path) > 1)

    def execute(self, context):
        space = context.space_data

        space.path.pop()

        return {'FINISHED'}


def get_socket_value(sock):
    if sock.type in {"VALUE", "INT", "BOOLEAN", "STRING"}:
        return sock.default_value
    elif sock.type in {"VECTOR", "RGBA"}:
        return tuple(sock.default_value)
    else:
        return None

def insert_socket_data(sock_data, sock):
    sock_data["name"] = sock.name
    sock_data["identifier"] = sock.identifier
    sock_data["default_value"] = get_socket_value(sock)
    sock_data["type"] = sock.type

def insert_group_interface(json_data, group):
    for sock in group.inputs:
        sock_data = {}
        insert_socket_data(sock_data, sock)
        sock_data["bl_socket_idname"] = sock.bl_socket_idname
        json_data["inputs"].append(sock_data)

    for sock in group.outputs:
        sock_data = {}
        insert_socket_data(sock_data, sock)
        sock_data["bl_socket_idname"] = sock.bl_socket_idname
        json_data["outputs"].append(sock_data)

def insert_node_data(node_data, node):
    node_data["name"] = node.name
    node_data["bl_idname"] = node.bl_idname
    node_data["location"] = tuple(map(int, node.location))
    node_data["width"] = int(node.width)

    # Don't compute this only once in the beginning, because addons might register more properties.
    base_node_property_names = {prop.identifier for prop in bpy.types.Node.bl_rna.properties}

    if node.bl_idname in {"NodeGroupInput", "NodeGroupOutput"}:
        pass
    elif node.bl_idname == "SimulationNodeGroup":
        node_data["group_name"] = getattr(node.node_tree, "name", None)
    else:
        node_data["properties"] = {}
        for prop in node.bl_rna.properties:
            if prop.identifier not in base_node_property_names:
                node_data["properties"][prop.identifier] = getattr(node, prop.identifier)

        node_data["inputs"] = []
        node_data["outputs"] = []
        for sock in node.inputs:
            sock_data = {}
            insert_socket_data(sock_data, sock)
            node_data["inputs"].append(sock_data)
        for sock in node.outputs:
            sock_data = {}
            insert_socket_data(sock_data, sock)
            node_data["outputs"].append(sock_data)

def insert_nodes_data(json_data, group):
    for node in group.nodes:
        node_data = {}
        insert_node_data(node_data, node)
        json_data["nodes"].append(node_data)

def insert_links_data(json_data, group):
    for link in group.links:
        link_data = {}
        link_data["from"] = [link.from_node.name, link.from_socket.identifier, list(link.from_node.outputs).index(link.from_socket)]
        link_data["to"] = [link.to_node.name, link.to_socket.identifier, list(link.to_node.inputs).index(link.to_socket)]
        json_data["links"].append(link_data)

def find_direct_dependency_groups(group):
    dependencies = set()
    for node in group.nodes:
        if node.bl_idname == "SimulationNodeGroup":
            if node.node_tree is not None:
                dependencies.add(node.node_tree)
    return dependencies

def insert_dependencies(json_data, group):
    dependency_names = [dependency.name for dependency in find_direct_dependency_groups(group)]
    json_data["dependencies"] = dependency_names

def node_group_to_json_data(group):
    json_data = {}
    json_data["blender_version"] = bpy.app.version
    json_data["name"] = group.name
    json_data["inputs"] = []
    json_data["outputs"] = []
    json_data["nodes"] = []
    json_data["links"] = []

    insert_group_interface(json_data, group)
    insert_nodes_data(json_data, group)
    insert_links_data(json_data, group)
    insert_dependencies(json_data, group)

    return json_data

def save_group_as_json(group, file_path):
    json_data = node_group_to_json_data(group)
    json_str = json.dumps(json_data, indent=1)
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    with open(file_path, "w") as f:
        f.write(json_str)


node_group_template_directory = os.path.join(bpy.utils.resource_path("LOCAL"), "datafiles", "node_groups")

def group_name_to_file_name(group_name):
    return group_name.replace(" ", "_") + ".json"

def file_name_to_group_name(file_name):
    return file_name.replace("_", " ")[:-len(".json")]

def group_name_to_file_path(group_name):
    file_name = group_name_to_file_name(group_name)
    return os.path.join(node_group_template_directory, file_name)

class NODE_OT_export_group_template(Operator):
    bl_idname = "node.export_group_template"
    bl_label = "Export Node Group Template"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.type == 'NODE_EDITOR' and getattr(space.edit_tree, "bl_idname", "") == "SimulationNodeTree"

    def execute(self, context):
        group = context.space_data.edit_tree
        file_path = group_name_to_file_path(group.name)
        save_group_as_json(group, file_path)
        return {'FINISHED'}

def property_has_given_value(owner, prop_name, value):
    prop = owner.bl_rna.properties[prop_name]
    if prop.type in {'BOOLEAN', 'INT', 'STRING', 'ENUM'}:
        return getattr(owner, prop_name) == value
    elif prop.type == 'FLOAT':
        if prop.array_length <= 1:
            return getattr(owner, prop_name) == value
        else:
            return tuple(getattr(owner, prop_name)) == tuple(value)
    else:
        assert False

def group_matches_json_data(group, json_data, loaded_group_by_name, json_data_by_group_name):
    if len(group.nodes) != len(json_data["nodes"]):
        return False
    if len(group.links) != len(json_data["links"]):
        return False
    if len(group.inputs) != len(json_data["inputs"]):
        return False
    if len(group.outputs) != len(json_data["outputs"]):
        return False

    for node_data in json_data["nodes"]:
        node = group.nodes.get(node_data["name"])
        if node is None:
            return False
        if node.bl_idname != node_data["bl_idname"]:
            return False

        if node.bl_idname in {"NodeGroupInput", "NodeGroupOutput"}:
            pass
        elif node.bl_idname == "SimulationNodeGroup":
            if node.node_tree is None:
                return False
            elif node.node_tree == loaded_group_by_name[node_data["group_name"]]:
                pass
            elif not group_matches_json_data(node.node_tree,
                                             json_data_by_group_name[node_data["group_name"]],
                                             loaded_group_by_name,
                                             json_data_by_group_name):
                return False

        else:
            for key, value in node_data["properties"].items():
                if getattr(node, key) != value:
                    return False
            if len(node.inputs) != len(node_data["inputs"]):
                return False
            if len(node.outputs) != len(node_data["outputs"]):
                return False
            for sock_data, sock in zip(node_data["inputs"], node.inputs):
                if hasattr(sock, "default_value"):
                    if not property_has_given_value(sock, "default_value", sock_data["default_value"]):
                        return False

    for sock_data, sock in zip(json_data["inputs"], group.inputs):
        if sock_data["bl_socket_idname"] != sock.bl_socket_idname:
            return False
        if sock_data["name"] != sock.name:
            return False

    for sock_data, sock in zip(json_data["outputs"], group.outputs):
        if sock_data["bl_socket_idname"] != sock.bl_socket_idname:
            return False
        if sock_data["name"] != sock.name:
            return False

    link_cache = set()
    for link in group.links:
        link_cache.add((
            link.from_node.name, list(link.from_node.outputs).index(link.from_socket),
            link.to_node.name, list(link.to_node.inputs).index(link.to_socket)))
    for link_data in json_data["links"]:
        if (link_data["from"][0], link_data["from"][2], link_data["to"][0], link_data["to"][2]) not in link_cache:
            return False

    return True

def get_or_create_node_group(json_data, loaded_group_by_name, json_data_by_group_name):
    for potential_group in bpy.data.node_groups:
        if group_matches_json_data(potential_group, json_data, loaded_group_by_name, json_data_by_group_name):
            return potential_group

    group = bpy.data.node_groups.new(json_data["name"], "SimulationNodeTree")

    for node_data in json_data["nodes"]:
        node = group.nodes.new(node_data["bl_idname"])
        node.name = node_data["name"]
        node.location = node_data["location"]
        node.select = False
        node.width = node_data["width"]

        if node.bl_idname in {"NodeGroupInput", "NodeGroupOutput"}:
            pass
        elif node.bl_idname == "SimulationNodeGroup":
            node.node_tree = loaded_group_by_name[node_data["group_name"]]
        else:
            for key, value in node_data["properties"].items():
                setattr(node, key, value)

            for sock_data, sock in zip(node_data["inputs"], node.inputs):
                if hasattr(sock, "default_value"):
                    sock.default_value = sock_data["default_value"]

    for sock_data in json_data["inputs"]:
        group.inputs.new(sock_data["bl_socket_idname"], sock_data["name"])

    for sock_data in json_data["outputs"]:
        group.outputs.new(sock_data["bl_socket_idname"], sock_data["name"])

    for link_data in json_data["links"]:
        from_socket = group.nodes[link_data["from"][0]].outputs[link_data["from"][2]]
        to_socket = group.nodes[link_data["to"][0]].inputs[link_data["to"][2]]
        group.links.new(from_socket, to_socket)

    return group

from collections import OrderedDict

def get_json_data_for_all_groups_to_load(main_group_name):
    json_data_by_group_name = OrderedDict()

    def load(group_name):
        if group_name in json_data_by_group_name:
            return
        else:
            file_path = group_name_to_file_path(group_name)
            with open(file_path, "r") as f:
                json_str = f.read()
            json_data = json.loads(json_str)
            assert group_name == json_data["name"]
            for dependency_name in json_data["dependencies"]:
                load(dependency_name)
            json_data_by_group_name[group_name] = json_data

    load(main_group_name)
    return json_data_by_group_name


class NODE_OT_import_group_template(Operator):
    bl_idname = "node.import_group_template"
    bl_label = "Import Node Group Template"
    bl_options = {'INTERNAL'}

    group_name: StringProperty()

    def execute(self, context):
        json_data_by_group_name = get_json_data_for_all_groups_to_load(self.group_name)
        loaded_group_by_name = dict()

        for group_name, json_data in json_data_by_group_name.items():
            group = get_or_create_node_group(json_data, loaded_group_by_name, json_data_by_group_name)
            loaded_group_by_name[group_name] = group

        return {'FINISHED'}

class NODE_OT_import_group_template_search(bpy.types.Operator):
    bl_idname = "node.import_group_template_search"
    bl_label = "Import Node Group Template Search"
    bl_property = "item"

    def get_group_name_items(self, context):
        if not os.path.exists(node_group_template_directory):
            return [('NONE', "None", "")]
        items = []
        for file_name in os.listdir(node_group_template_directory):
            if not file_name.endswith(".json"):
                continue
            group_name = file_name_to_group_name(file_name)
            items.append((file_name, group_name, ""))
        if len(items) == 0:
            items.append(('NONE', "None", ""))
        return items


    item: EnumProperty(items=get_group_name_items)

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        if self.item == "NONE":
            return {'CANCELLED'}
        else:
            group_name = file_name_to_group_name(self.item)
            bpy.ops.node.import_group_template(group_name=group_name)
            return {'FINISHED'}

classes = (
    NodeSetting,

    NODE_OT_add_and_link_node,
    NODE_OT_add_node,
    NODE_OT_add_search,
    NODE_OT_collapse_hide_unused_toggle,
    NODE_OT_tree_path_parent,
    NODE_OT_export_group_template,
    NODE_OT_import_group_template,
    NODE_OT_import_group_template_search,
)
