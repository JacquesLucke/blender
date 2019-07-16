import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_builder import SocketBuilder

class CustomEmitterNode(bpy.types.Node, FunctionNode):
    bl_idname = "bp_CustomEmitterNode"
    bl_label = "Custom Emitter"

    function_tree: PointerProperty(
        name="Function Tree",
        type=bpy.types.NodeTree,
        update=FunctionNode.refresh,
    )

    def declaration(self, builder: SocketBuilder):
        if self.function_tree:
            builder.tree_interface_input("inputs", self.function_tree, 'IN',
                ignored={("Float", "Time Step"), ("Float", "Start Time")})

        builder.emitter_output("emitter", "Emitter")

    def draw(self, layout):
        layout.prop(self, "function_tree", text="")

    def iter_dependency_trees(self):
        if self.function_tree is not None:
            yield self.function_tree
