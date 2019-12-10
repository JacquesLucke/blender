import bpy
from bpy.props import *
from .. base import SimulationNode, FunctionNode
from .. node_builder import NodeBuilder

class SetParticleAttributeNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SetParticleAttributeNode"
    bl_label = "Set Particle Attribute"

    attribute_name: StringProperty(
        name="Attribute Name",
        default="My Attribute",
    )

    attribute_type: StringProperty(
        name="Attribute Type",
        default="Float",
        update=SimulationNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("value", "Value", self.attribute_type)
        builder.execute_output("execute", "Execute")

    def draw(self, layout):
        row = layout.row(align=True)
        row.prop(self, "attribute_name", text="")
        self.invoke_type_selection(row, "set_type", "", mode="BASE", icon="SETTINGS")

    def set_type(self, data_type):
        self.attribute_type = data_type


class GetParticleAttribute(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetParticleAttributeNode"
    bl_label = "Get Particle Attribute"

    attribute_name: StringProperty(
        name="Attribute Name",
        default="My Attribute",
    )

    attribute_type: StringProperty(
        name="Attribute Type",
        default="Float",
        update=SimulationNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("value", "Value", self.attribute_type)

    def draw(self, layout):
        row = layout.row(align=True)
        row.prop(self, "attribute_name", text="")
        self.invoke_type_selection(row, "set_type", "", mode="BASE", icon="SETTINGS")

    def set_type(self, data_type):
        self.attribute_type = data_type
