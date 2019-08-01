import bpy
from bpy.props import *
from . types import type_infos

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
    def create_input(tree):
        input_node = tree.nodes.new("fn_FunctionInputNode")
        variadic = input_node.outputs[0].get_decl(input_node)
        for data_type, name in inputs:
            variadic.add_item(data_type, name)
        return input_node

    def create_output(tree):
        output_node = tree.nodes.new("fn_FunctionOutputNode")
        variadic = output_node.inputs[0].get_decl(output_node)
        for data_type, name in outputs:
            variadic.add_item(data_type, name)
        return output_node

    tree = bpy.data.node_groups.new(name, "FunctionTree")
    input_node = create_input(tree)
    output_node = create_output(tree)
    input_node.location.x = -200 - input_node.width
    output_node.location.x = 200
    tree.sync()
    return tree
