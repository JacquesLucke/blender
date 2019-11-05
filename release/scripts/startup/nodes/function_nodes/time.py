import bpy
from .. node_builder import NodeBuilder
from .. base import FunctionNode

class TimeInfoNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_TimeInfoNode"
    bl_label = "Time Info"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("frame", "Frame", "Float")
