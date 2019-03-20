import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import PackListDecl, FixedSocketDecl
from .. sockets import type_infos

class PackListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_PackListNode"
    bl_label = "Pack List"

    active_type: StringProperty(default="Float")
    variadic: PackListDecl.Property()

    def get_sockets(self):
        return [
            PackListDecl("inputs", "variadic", self.active_type),
        ], [
            FixedSocketDecl("output", "List", type_infos.to_list(self.active_type)),
        ]

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Change Type", mode="BASE")

    def set_type(self, data_type):
        self.active_type = data_type
        self.rebuild_and_try_keep_state()