import bpy
import uuid
from bpy.props import *
from .. base import SimulationNode, DataSocket
from .. node_builder import NodeBuilder
from .. types import type_infos


class CustomEmitterAttribute(bpy.types.PropertyGroup):
    def sync_tree(self, context):
        self.id_data.sync()

    attribute_name: StringProperty(update=sync_tree)
    attribute_type: StringProperty(update=sync_tree)
    identifier: StringProperty()
    is_list: NodeBuilder.VectorizedProperty()


class CustomEmitter(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CustomEmitterNode"
    bl_label = "Custom Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    attributes: CollectionProperty(
        type=CustomEmitterAttribute,
    )

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
        self.invoke_type_selection(layout, "add_attribute", "Add Attribute", mode="BASE")

    def draw_socket(self, layout, socket, text, decl, index_in_decl):
        if isinstance(socket, DataSocket):
            index = list(self.inputs).index(socket)
            item = self.attributes[index]
            col = layout.column(align=True)
            row = col.row(align=True)
            row.prop(item, "attribute_name", text="")
            self.invoke_type_selection(row, "set_attribute_type", "", icon="SETTINGS", mode="BASE", settings=(index, ))
            if hasattr(socket, "draw_property"):
                socket.draw_property(col, self, "")
        else:
            decl.draw_socket(layout, socket, index_in_decl)

    def add_attribute(self, data_type):
        item = self.attributes.add()
        item.identifier = str(uuid.uuid4())
        item.attribute_name = "My Attribute"
        item.attribute_type = data_type

    def set_attribute_type(self, data_type, index):
        self.attributes[index].attribute_type = data_type
