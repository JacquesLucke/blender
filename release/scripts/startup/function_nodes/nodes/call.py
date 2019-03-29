import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import TreeInterfaceDecl

class CallNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_CallNode"
    bl_label = "Call"

    function_tree: PointerProperty(
        name="Function Tree",
        type=bpy.types.NodeTree,
        update=FunctionNode.refresh,
    )

    def get_sockets(self):
        if self.function_tree is None:
            return [], []

        return [
            TreeInterfaceDecl(self, "inputs", self.function_tree, "IN"),
        ], [
            TreeInterfaceDecl(self, "outputs", self.function_tree, "OUT"),
        ]

    def draw(self, layout):
        layout.prop(self, "function_tree", text="")