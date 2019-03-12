import bpy
from .. base import BaseNode
from .. sockets import info

class FunctionInputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionInputNode"
    bl_label = "Function Input"

    def init(self, context):
        pass

    def draw(self, layout):
        col = layout.column(align=True)
        self.invoke_function(col, "new_socket",
            "New Float", settings=("Float", ))
        self.invoke_function(col, "new_socket",
            "New Integer", settings=("Integer", ))
        self.invoke_function(col, "new_socket",
            "New Vector", settings=("Vector", ))
        self.invoke_function(col, "new_socket",
            "New Float List", settings=("Float List", ))

    def draw_socket(self, socket, layout, text):
        row = layout.row(align=True)
        row.prop(socket, "name", text="")

        index = list(self.outputs).index(socket)
        self.invoke_function(row, "remove_socket",
            text="", icon="X", settings=(index, ))

    def new_socket(self, data_type):
        info.build(data_type, self.outputs, "Input")

    def remove_socket(self, index):
        self.outputs.remove(self.outputs[index])
