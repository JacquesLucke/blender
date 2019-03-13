import bpy
from .. base import BaseNode
from .. sockets import type_infos

class FunctionOutputNode(BaseNode, bpy.types.Node):
    bl_idname = "fn_FunctionOutputNode"
    bl_label = "Function Output"

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

        index = list(self.inputs).index(socket)
        self.invoke_function(row, "remove_socket",
            text="", icon="X", settings=(index, ))

    def new_socket(self, data_type):
        type_infos.build(data_type, self.inputs, "Output")

    def remove_socket(self, index):
        self.inputs.remove(self.inputs[index])