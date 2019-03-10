import bpy
from .. base import BaseNode

class FunctionOutputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionOutputNode"
    bl_label = "Function Output"

    def init(self, context):
        pass

    def draw(self, layout):
        col = layout.column(align=True)
        self.invoke_function(col, "new_socket",
            "New Float", settings=("fn_FloatSocket", ))
        self.invoke_function(col, "new_socket",
            "New Integer", settings=("fn_IntegerSocket", ))
        self.invoke_function(col, "new_socket",
            "New Vector", settings=("fn_VectorSocket", ))

    def draw_socket(self, socket, layout, text):
        row = layout.row(align=True)
        row.prop(socket, "name", text="")

        index = list(self.inputs).index(socket)
        self.invoke_function(row, "remove_socket",
            text="", icon="X", settings=(index, ))

    def new_socket(self, idname):
        self.inputs.new(idname, "Output")

    def remove_socket(self, index):
        self.inputs.remove(self.inputs[index])