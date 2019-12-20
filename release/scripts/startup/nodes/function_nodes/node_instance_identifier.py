import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder


class NodeInstanceIdentifierNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_NodeInstanceIdentifierNode"
    bl_label = "Node Instance Identifier"

    def declaration(self, builder):
        builder.fixed_output("identifier", "Identifier", "Text")
