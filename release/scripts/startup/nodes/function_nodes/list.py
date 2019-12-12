import bpy
from bpy.props import *
from .. types import type_infos
from .. base import FunctionNode
from .. node_builder import NodeBuilder


class GetListElementNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementNode"
    bl_label = "Get List Element"

    active_type: NodeBuilder.DynamicListProperty()

    def declaration(self, builder: NodeBuilder):
        builder.dynamic_list_input("list", "List", "active_type")
        builder.fixed_input("index", "Index", "Integer")
        builder.dynamic_base_input("fallback", "Fallback", "active_type")
        builder.dynamic_base_output("value", "Value", "active_type")


class GetListElementsNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetListElementsNode"
    bl_label = "Get List Elements"

    active_type: NodeBuilder.DynamicListProperty()

    def declaration(self, builder: NodeBuilder):
        builder.dynamic_list_input("list", "List", "active_type")
        builder.fixed_input("indices", "Indices", "Integer List")
        builder.dynamic_base_input("fallback", "Fallback", "active_type")
        builder.dynamic_list_output("values", "Values", "active_type")


class ListLengthNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ListLengthNode"
    bl_label = "List Length"

    active_type: NodeBuilder.DynamicListProperty()

    def declaration(self, builder: NodeBuilder):
        builder.dynamic_list_input("list", "List", "active_type")
        builder.fixed_output("length", "Length", "Integer")


class PackListNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_PackListNode"
    bl_label = "Pack List"

    active_type: StringProperty(
        default="Float",
        update=FunctionNode.sync_tree)

    variadic: NodeBuilder.BaseListVariadicProperty()

    def declaration(self, builder):
        builder.base_list_variadic_input("inputs", "variadic", self.active_type)
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
