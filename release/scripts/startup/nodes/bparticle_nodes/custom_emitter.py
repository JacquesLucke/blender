import bpy
import uuid
from bpy.props import *
from .. base import SimulationNode, DataSocket, FunctionNode
from .. node_builder import NodeBuilder
from .. types import type_infos
from .. sync import skip_syncing


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

    birth_time_mode: EnumProperty(
        name="Birth Time Mode",
        items=[
            ("NONE", "None", "Manually specify birth times of every particle", "NONE", 0),
            ("BEGIN", "Begin", "Spawn particles at the beginning of each time step", "NONE", 1),
            ("END", "End", "Spawn particles at the end of each time step", "NONE", 2),
            ("RANDOM", "Random", "Spawn particles at random moments in the time step", "NONE", 3),
            ("LINEAR", "Linear", "Distribute particles linearly in each time step", "NONE", 4),
        ],
        default="END",
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
        layout.prop(self, "birth_time_mode", text="Birth")
        self.invoke_type_selection(layout, "add_attribute", "Add Attribute", mode="BASE")

    def draw_socket(self, layout, socket, text, decl, index_in_decl):
        if isinstance(socket, DataSocket):
            index = list(self.inputs).index(socket)
            item = self.attributes[index]
            col = layout.column(align=True)
            row = col.row(align=True)
            row.prop(item, "attribute_name", text="")
            self.invoke_type_selection(row, "set_attribute_type", "", icon="SETTINGS", mode="BASE", settings=(index, ))
            self.invoke_function(row, "remove_attribute", "", icon="X", settings=(index, ))
            if hasattr(socket, "draw_property"):
                socket.draw_property(col, self, "")
        else:
            decl.draw_socket(layout, socket, index_in_decl)

    def add_attribute(self, data_type):
        with skip_syncing():
            item = self.attributes.add()
            item.identifier = str(uuid.uuid4())
            item.attribute_type = data_type
            item.attribute_name = "My Attribute"

        self.sync_tree()

    def remove_attribute(self, index):
        self.attributes.remove(index)
        self.sync_tree()

    def set_attribute_type(self, data_type, index):
        self.attributes[index].attribute_type = data_type

class EmitterTimeInfoNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_EmitterTimeInfoNode"
    bl_label = "Emitter Time Info"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("duration", "Duration", "Float")
        builder.fixed_output("begin", "Begin", "Float")
        builder.fixed_output("end", "End", "Float")
        builder.fixed_output("step", "Step", "Integer")
