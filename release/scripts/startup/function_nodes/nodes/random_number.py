import bpy
from .. base import FunctionNode
from .. socket_decl import FixedSocketDecl

class RandomNumberNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_RandomNumberNode"
    bl_label = "Random Number"

    def get_sockets(self):
        return [
            FixedSocketDecl(self, "seed", "Seed", "Integer"),
            FixedSocketDecl(self, "min", "Min", "Float"),
            FixedSocketDecl(self, "max", "Max", "Float"),
        ], [
            FixedSocketDecl(self, "value", "Value", "Float"),
        ]