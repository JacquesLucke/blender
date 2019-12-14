import bpy
from bpy.props import *
from .. base import SimulationNode, FunctionNode
from .. node_builder import NodeBuilder

class SetParticleAttributeNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SetParticleAttributeNode"
    bl_label = "Set Particle Attribute"

    attribute_type: StringProperty(
        name="Attribute Type",
        default="Float",
        update=SimulationNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text", default="My Attribute", display_name=False)
        builder.fixed_input("value", "Value", self.attribute_type)
        builder.execute_output("execute", "Execute")

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Select Type", mode="BASE", icon="SETTINGS")

    def set_type(self, data_type):
        self.attribute_type = data_type


class GetParticleAttribute(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetParticleAttributeNode"
    bl_label = "Get Particle Attribute"

    attribute_type: StringProperty(
        name="Attribute Type",
        default="Float",
        update=SimulationNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text", default="My Attribute", display_name=False)
        builder.fixed_output("value", "Value", self.attribute_type)

    def draw(self, layout):
        self.invoke_type_selection(layout, "set_type", "Select Type", mode="BASE", icon="SETTINGS")

    def set_type(self, data_type):
        self.attribute_type = data_type
