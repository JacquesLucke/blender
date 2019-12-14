import bpy
import uuid
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class SwitchNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SwitchNode"
    bl_label = "Switch"

    data_type: StringProperty(
        default="Float",
        update=FunctionNode.sync_tree
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.fixed_input("true", "True", self.data_type)
        builder.fixed_input("false", "False", self.data_type)
        builder.fixed_output("result", "Result", self.data_type)

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Change Type")

    def set_type(self, data_type):
        self.data_type = data_type


class SelectNodeItem(bpy.types.PropertyGroup):
    identifier: StringProperty()

class SelectNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_SelectNode"
    bl_label = "Select"

    data_type: StringProperty(
        default="Float",
        update=FunctionNode.sync_tree,
    )

    input_items: CollectionProperty(
        type=SelectNodeItem,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("select", "Select", "Integer")
        for i, item in enumerate(self.input_items):
            builder.fixed_input(item.identifier, str(i), self.data_type)
        builder.fixed_input("fallback", "Fallback", self.data_type)
        builder.fixed_output("result", "Result", self.data_type)

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Change Type")
        self.invoke_function(layout, "add_input", "Add Input")

    def set_type(self, data_type):
        self.data_type = data_type

    def add_input(self):
        item = self.input_items.add()
        item.identifier = str(uuid.uuid4())
        self.sync_tree()
