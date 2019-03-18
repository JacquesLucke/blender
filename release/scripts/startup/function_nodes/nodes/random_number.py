import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class RandomNumberNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_RandomNumberNode"
    bl_label = "Random Number"

    def get_sockets(self):
        return [
            FixedSocketDecl("seed", "Seed", "Integer"),
            FixedSocketDecl("min", "Min", "Float"),
            FixedSocketDecl("max", "Max", "Float"),
        ], [
            FixedSocketDecl("value", "Value", "Float"),
        ]