import bpy
from bpy.props import *
from .. base import FunctionNode
from .. types import type_infos
from .. node_builder import NodeBuilder

class PackListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_PackListNode"
    bl_label = "Pack List"

    active_type: StringProperty(
        default="Float",
        update=FunctionNode.refresh)

    variadic: NodeBuilder.PackListProperty()

    def declaration(self, builder):
        builder.pack_list_input("inputs", "variadic", self.active_type)
        builder.fixed_output("output", "List", type_infos.to_list(self.active_type))

    def draw_advanced(self, layout):
        self.invoke_type_selection(layout, "set_type", "Change Type", mode="BASE")

    def set_type(self, data_type):
        self.active_type = data_type

    @classmethod
    def get_search_terms(cls):
        for list_type in type_infos.iter_list_types():
            base_type = type_infos.to_base(list_type)
            yield ("Pack " + list_type, {"active_type" : base_type})
