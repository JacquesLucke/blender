import bpy
from .. node_builder import NodeBuilder
from .. base import FunctionNode

class TextLengthNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_TextLengthNode"
    bl_label = "Text Length"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("text", "Text", "Text")
        builder.fixed_output("length", "Length", "Integer")
