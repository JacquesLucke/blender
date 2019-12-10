import bpy
import uuid
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder
from .. types import type_infos


class CustomEmitterAttribute(bpy.types.PropertyGroup):
    attribute_name: StringProperty()
    attribute_type: StringProperty()
    identifier: StringProperty()
    is_list: NodeBuilder.VectorizedProperty()


class CustomEmitter(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CustomEmitterNode"
    bl_label = "Custom Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    attributes: CollectionProperty(
        type=CustomEmitterAttribute,
    )

    new_attribute_name: StringProperty(default="My Attribute")

    def declaration(self, builder: NodeBuilder):
        for i, item in enumerate(self.attributes):
            builder.vectorized_input(
                item.identifier,
                f"attributes[{i}].is_list",
                item.attribute_name,
                item.attribute_name,
                item.attribute_type)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.influences_output("emitter", "Emitter")

    def draw(self, layout):
        layout.prop(self, "new_attribute_name")
        self.invoke_type_selection(layout, "add_attribute", "Add Attribute", mode="BASE")

    def add_attribute(self, data_type):
        item = self.attributes.add()
        item.identifier = str(uuid.uuid4())
        item.attribute_name = self.new_attribute_name
        item.attribute_type = data_type
        self.sync_tree()
