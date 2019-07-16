import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_builder import SocketBuilder
from .. node_operators import new_function_tree

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
        row = layout.row(align=True)
        row.prop(self, "function_tree", text="")
        if self.function_tree is None:
            self.invoke_function(row, "new_function", "", icon='PLUS')

    def new_function(self):
        self.function_tree = new_function_tree("Custom Emitter", [
            ("Float", "Start Time"),
            ("Float", "Time Step"),
        ], [
            ("Vector List", "Position"),
            ("Vector List", "Velocity"),
            ("Float List", "Size"),
        ])

    def iter_dependency_trees(self):
        if self.function_tree is not None:
            yield self.function_tree
