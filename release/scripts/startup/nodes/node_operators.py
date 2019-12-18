import bpy
from bpy.props import *
from . types import type_infos
from . function_tree import FunctionTree

def try_find_node(tree_name, node_name):
    tree = bpy.data.node_groups.get(tree_name)
    if tree is not None:
        return tree.nodes.get(node_name)
    return None

class NodeOperatorBase:
    tree_name: StringProperty()
    node_name: StringProperty()
    function_name: StringProperty()
    settings_repr: StringProperty()

    def call(self, *args):
        node = try_find_node(self.tree_name, self.node_name)
        if node is None:
            return {'CANCELLED'}

        function = getattr(node, self.function_name)
        settings = eval(self.settings_repr)
        function(*args, *settings)
        return {'FINISHED'}

class NodeOperator(bpy.types.Operator, NodeOperatorBase):
    bl_idname = "fn.node_operator"
    bl_label = "Generic Node Operator"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        return self.call()

class NodeDataTypeSelector(bpy.types.Operator, NodeOperatorBase):
    bl_idname = "fn.node_data_type_selector"
    bl_label = "Generic Node Data Type Selector"
    bl_options = {'INTERNAL'}
    bl_property = "item"

    mode: EnumProperty(
        items=[
            ("ALL", "All", ""),
            ("BASE", "Base", ""),
        ])

    def get_items(self, context):
        if self.mode == "ALL":
            return type_infos.get_data_type_items()
        elif self.mode == "BASE":
            return type_infos.get_base_type_items()
        else:
            assert False

    item: EnumProperty(items=get_items)

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        return self.call(self.item)

class NodeGroupSelector(bpy.types.Operator, NodeOperatorBase):
    bl_idname = "fn.node_group_selector"
    bl_label = "Node Group Selector"
    bl_options = {'INTERNAL'}
    bl_property = "item"

    def get_items(self, context):
        tree = bpy.data.node_groups.get(self.tree_name)
        possible_trees = tree.find_callable_trees()

        items = []
        for tree in possible_trees:
            items.append((tree.name, tree.name, ""))
        items.append(("NONE", "None", ""))
        return items

    item: EnumProperty(items=get_items)

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        if self.item == "NONE":
            return self.call(None)
        else:
            return self.call(bpy.data.node_groups.get(self.item))

class MoveViewToNode(bpy.types.Operator):
    bl_idname = "fn.move_view_to_node"
    bl_label = "Move View to Node"
    bl_options = {'INTERNAL'}

    tree_name: StringProperty()
    node_name: StringProperty()

    def execute(self, context):
        target_node = try_find_node(self.tree_name, self.node_name)
        if target_node is None:
            return {'CANCELLED'}

        tree = target_node.tree
        context.space_data.node_tree = tree
        for node in tree.nodes:
            node.select = False

        target_node.select = True
        tree.nodes.active = target_node

        bpy.ops.node.view_selected('INVOKE_DEFAULT')
        return {'FINISHED'}

def new_function_tree(name, inputs, outputs):
    tree = bpy.data.node_groups.new(name, "FunctionTree")

    for i, (data_type, input_name) in enumerate(inputs):
        input_node = tree.nodes.new("fn_GroupInputNode")
        input_node.sort_index = i
        input_node.interface_type = "DATA"
        input_node.input_name = input_name
        input_node.data_type = data_type
        input_node.location = (-200, -i * 130)

    for i, (data_type, output_name) in enumerate(outputs):
        output_node = tree.nodes.new("fn_GroupOutputNode")
        output_node.sort_index = i
        output_node.output_name = output_name
        output_node.data_type = data_type
        output_node.location = (200, -i * 130)

    tree.sync()
    return tree

class NewParticleSystem(bpy.types.Operator):
    bl_idname = "fn.new_particle_system"
    bl_label = "New Particle System"

    def execute(self, context):
        mesh = bpy.data.meshes.new("Particle Simulation")
        ob = bpy.data.objects.new("Particle Simulation", mesh)
        modifier = ob.modifiers.new("BParticles", 'BPARTICLES')

        bpy.ops.fn.new_particle_simulation_tree(object_name=ob.name, modifier_name=modifier.name)

        context.collection.objects.link(ob)

        return {'FINISHED'}
