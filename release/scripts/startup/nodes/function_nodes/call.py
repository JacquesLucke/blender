import bpy
from bpy.props import *
from .. base import FunctionNode

class CallNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CallNode"
    bl_label = "Call"

    function_tree: PointerProperty(
        name="Function Tree",
        type=bpy.types.NodeTree,
        update=FunctionNode.refresh,
    )

    def declaration(self, builder):
        if self.function_tree is None:
            return
        builder.tree_interface_input("inputs", self.function_tree, "IN")
        builder.tree_interface_output("outputs", self.function_tree, "OUT")

    def draw(self, layout):
        layout.prop(self, "function_tree", text="")

    def iter_dependency_trees(self):
        if self.function_tree is not None:
            yield self.function_tree
