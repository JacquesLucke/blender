import bpy
from . base import DataSocket, BaseSocket
from bpy.props import *

class OperatorSocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "fn_OperatorSocket"
    bl_label = "Operator Socket"

    def draw_color(self, context, node):
        return (0, 0, 0, 0)

class FloatSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_FloatSocket"
    bl_label = "Float Socket"
    data_type = "Float"
    color = (0, 0.3, 0.5, 1)

    value: FloatProperty(
        name="Value",
        default=0.0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class IntegerSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_IntegerSocket"
    bl_label = "Integer Socket"
    data_type = "Integer"
    color = (0.3, 0.7, 0.5, 1)

    value: IntProperty(
        name="Value",
        default=0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class VectorSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_VectorSocket"
    bl_label = "Vector Socket"
    data_type = "Vector"
    color = (0, 0, 0.5, 1)

    value: FloatVectorProperty(
        name="Value",
        size=3,
        default=(0.0, 0.0, 0.0),
    )

    def draw_property(self, layout, node, text):
        layout.column().prop(self, "value", text=text)

    def get_state(self):
        return tuple(self.value)

    def restore_state(self, state):
        self.value = state

class BooleanSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_BooleanSocket"
    bl_label = "Boolean Socket"
    data_type = "Boolean"
    color = (0.3, 0.3, 0.3, 1)

    value: BoolProperty(
        name="Value",
        default=False,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class ObjectSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_ObjectSocket"
    bl_label = "Object Socket"
    data_type = "Object"
    color = (0, 0, 0, 1)

    value: PointerProperty(
        name="Value",
        type=bpy.types.Object,
    )

    display_name: BoolProperty(default=True)

    def draw_property(self, layout, node, text):
        if not self.display_name:
            text = ""
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class ImageSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_ImageSocket"
    bl_label = "Image Socket"
    data_type = "Image"
    color = (0.6, 0.6, 0.6, 1)

    value: PointerProperty(
        name="Value",
        type=bpy.types.Image,
    )

    display_name: BoolProperty()

    def draw_property(self, layout, node, text):
        if not self.display_name:
            text = ""
        layout.prop(self, "value", text=text)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

class ColorSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_ColorSocket"
    bl_label = "Color Socket"
    data_type = "Color"
    color = (0.8, 0.8, 0.2, 1)

    value: FloatVectorProperty(
        name="Value",
        size=4,
        default=(0.8, 0.8, 0.8, 1.0),
        subtype='COLOR',
        soft_min=0.0,
        soft_max=0.0,
    )

    def draw_property(self, layout, node, text):
        layout.prop(self, "value", text=text)

    def get_state(self):
        return tuple(self.value)

    def restore_state(self, state):
        self.value = state

class TextSocket(bpy.types.NodeSocket, DataSocket):
    bl_idname = "fn_TextSocket"
    bl_label = "Text Socket"
    data_type = "Text"
    color = (0.8, 0.8, 0.8, 1)

    value: StringProperty(
        name="Value",
        default="",
    )

    display_name: BoolProperty(default=True)
    display_icon: StringProperty(default="NONE")

    def draw_property(self, layout, node, text):
        if not self.display_name:
            text = ""
        layout.prop(self, "value", text=text, icon=self.display_icon)

    def get_state(self):
        return self.value

    def restore_state(self, state):
        self.value = state

def create_simple_data_socket(idname, data_type, color):
    return type(idname, (bpy.types.NodeSocket, DataSocket),
        {
            "bl_idname" : idname,
            "bl_label" : idname,
            "data_type" : data_type,
            "color" : color,
        })

FloatListSocket = create_simple_data_socket(
    "fn_FloatListSocket", "Float List", (0, 0.3, 0.5, 0.5))
VectorListSocket = create_simple_data_socket(
    "fn_VectorListSocket", "Vector List", (0, 0, 0.5, 0.5))
IntegerListSocket = create_simple_data_socket(
    "fn_IntegerListSocket", "Integer List", (0.3, 0.7, 0.5, 0.5))
BooleanListSocket = create_simple_data_socket(
    "fn_BooleanListSocket", "Boolean List", (0.3, 0.3, 0.3, 0.5))
ObjectListSocket = create_simple_data_socket(
    "fn_ObjectListSocket", "Object List", (0, 0, 0, 0.5))
ImageListSocket = create_simple_data_socket(
    "fn_ImageListSocket", "Image List", (0.6, 0.6, 0.6, 0.5))
ColorListSocket = create_simple_data_socket(
    "fn_ColorListSocket", "Color List", (0.8, 0.8, 0.2, 0.5))
TextListSocket = create_simple_data_socket(
    "fn_TextListSocket", "Text List", (0.8, 0.8, 0.8, 0.5))
SurfaceHookSocket = create_simple_data_socket(
    "fn_SurfaceHookSocket", "Surface Hook", (0.2, 0.8, 0.2, 1.0))
SurfaceHookListSocket = create_simple_data_socket(
    "fn_SurfaceHookListSocket", "Surface Hook List", (0.2, 0.8, 0.2, 0.5))

class ExecuteSocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "fn_ExecuteSocket"
    bl_label = "Control Flow Socket"
    color = (0.8, 0.2, 0.2, 1)

class InfluencesSocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "fn_InfluencesSocket"
    bl_label = "Influences Socket"
    color = (0.8, 0.8, 0.2, 1)

def make_mockup_socket(idname, color, shape):
    if len(color) == 3:
        color = list(color) + [1.0]
    return type(idname, (bpy.types.NodeSocket, BaseSocket),
        {
            "bl_idname" : idname,
            "bl_label" : idname,
            "color" : color,
            "default_shape" : shape,
        })

SimulationsSocket = make_mockup_socket(
    "fn_SimulationsSocket", (0.2, 0.3, 0.6), "DIAMOND")

SolverSocket = make_mockup_socket(
    "fn_SolverSocket", (0.2, 0.8, 0.2), "SQUARE")
EmittersSocket = make_mockup_socket(
    "fn_EmittersSocket", (0.8, 0.8, 0.2), "DIAMOND")
EventsSocket = make_mockup_socket(
    "fn_EventsSocket", (0.8, 0.2, 0.8), "DIAMOND")
ForcesSocket = make_mockup_socket(
    "fn_ForcesSocket", (0.2, 0.8, 0.8), "DIAMOND")
CollidersSocket = make_mockup_socket(
    "fn_CollidersSocket", (0.4, 0.6, 0.8), "DIAMOND")
MeshSocket = make_mockup_socket(
    "fn_MeshSocket", (0.4, 0.8, 0.3), "CIRCLE")


class GeometrySocket(bpy.types.NodeSocket, BaseSocket):
    bl_idname = "fn_GeometrySocket"
    bl_label = "Geometry Socket"
    color = (0.5, 0.5, 0.5, 1)
    default_shape = "CIRCLE"

    value: PointerProperty(type=bpy.types.Object)

    def draw_self(self, layout, node, text):
        if not (self.is_linked or self.is_output):
            layout.prop(self, "value", text=text)
        else:
            layout.label(text=text)
