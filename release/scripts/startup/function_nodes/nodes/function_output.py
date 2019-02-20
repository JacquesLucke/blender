import bpy
from .. base import BaseNode

class FunctionOutputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionOutputNode"
    bl_label = "Function Output"

    def init(self, context):
        pass

    def draw(self, layout):
        self.invoke_function(layout, "new_socket",
            "New Float", settings=("fn_FloatSocket", ))
        self.invoke_function(layout, "new_socket",
            "New Vector", settings=("fn_VectorSocket", ))

    def new_socket(self, idname):
        self.inputs.new(idname, "Output")

bpy.utils.register_class(FunctionOutputNode)