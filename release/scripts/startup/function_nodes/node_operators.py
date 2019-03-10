import bpy
from bpy.props import *

class NodeOperator(bpy.types.Operator):
    bl_idname = "fn.node_operator"
    bl_label = "Generic Node Operator"
    bl_options = {"INTERNAL"}

    tree_name: StringProperty()
    node_name: StringProperty()
    function_name: StringProperty()
    settings_repr: StringProperty()

    def execute(self, context):
        tree = bpy.data.node_groups.get(self.tree_name)
        if tree is None:
            return {"CANCELLED"}

        node = tree.nodes.get(self.node_name)
        if node is None:
            return {"CANCELLED"}

        function = getattr(node, self.function_name)
        settings = eval(self.settings_repr)
        function(*settings)

        return {"FINISHED"}