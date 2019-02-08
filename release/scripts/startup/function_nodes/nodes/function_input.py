import bpy

class FunctionInputNode(bpy.types.Node):
    bl_idname = "fn_FunctionInputNode"
    bl_label = "Function Input"

    def init(self, context):
        self.outputs.new("fn_VectorSocket", "Position")
        self.outputs.new("fn_FloatSocket", "Control")

bpy.utils.register_class(FunctionInputNode)