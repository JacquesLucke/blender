import bpy
from .. base import BaseNode

class FunctionInputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionInputNode"
    bl_label = "Function Input"

    def init(self, context):
        self.outputs.new("fn_VectorSocket", "Position")
        self.outputs.new("fn_FloatSocket", "Control")

bpy.utils.register_class(FunctionInputNode)