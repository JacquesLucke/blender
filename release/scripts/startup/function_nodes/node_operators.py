import bpy
from bpy.props import *
from . sockets import type_infos

class NodeOperatorBase:
    tree_name: StringProperty()
    node_name: StringProperty()
    function_name: StringProperty()
    settings_repr: StringProperty()

    def call(self, *args):
        tree = bpy.data.node_groups.get(self.tree_name)
        if tree is None:
            return {'CANCELLED'}

        node = tree.nodes.get(self.node_name)
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

