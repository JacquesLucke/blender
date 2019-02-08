import bpy

class FunctionOutputNode(bpy.types.Node):
    bl_idname = "fn_FunctionOutputNode"
    bl_label = "Function Output"

    def init(self, context):
        self.inputs.new("fn_VectorSocket", "Position")

bpy.utils.register_class(FunctionOutputNode)