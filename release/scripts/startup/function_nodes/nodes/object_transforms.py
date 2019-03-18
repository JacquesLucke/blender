import bpy
from bpy.props import *
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class ObjectTransformsNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ObjectTransformsNode"
    bl_label = "Object Transforms"

    object: PointerProperty(
        name="Object",
        type=bpy.types.Object,
    )

    def get_sockets(self):
        return [], [
            FixedSocketDecl("location", "Location", "Vector"),
        ]

    def draw(self, layout):
        layout.prop(self, "object", text="")