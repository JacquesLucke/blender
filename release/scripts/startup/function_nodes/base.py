import bpy
from . utils.generic import iter_subclasses_recursive

class FunctionNodeTree(bpy.types.NodeTree):
    bl_idname = "FunctionNodeTree"
    bl_icon = "MOD_DATA_TRANSFER"
    bl_label = "Function Nodes"


class BaseNode:
    def draw_buttons(self, context, layout):
        self.draw(layout)

    def draw(self, layout):
        pass

    def invoke_function(self,
            layout, function_name, text,
            *, icon="NONE", settings=tuple()):
        assert isinstance(settings, tuple)
        props = layout.operator("fn.node_operator", text=text, icon=icon)
        props.tree_name = self.id_data.name
        props.node_name = self.name
        props.function_name = function_name
        props.settings_repr = repr(settings)

    def draw_socket(self, socket, layout, text):
        socket.draw_self(layout, self, text)

    @classmethod
    def iter_final_subclasses(cls):
        yield from filter(lambda x: issubclass(x, bpy.types.Node), iter_subclasses_recursive(cls))

class BaseSocket:
    color = (0, 0, 0, 0)

    def draw_color(self, context, node):
        return self.color

    def draw(self, context, layout, node, text):
        node.draw_socket(self, layout, text)

    def draw_self(self, layout, node, text):
        layout.label(text=text)

class FunctionNode(BaseNode):
    def init(self, context):
        inputs, outputs = self.get_sockets()
        for idname, name in inputs:
            self.inputs.new(idname, name)
        for idname, name in outputs:
            self.outputs.new(idname, name)

    def get_sockets():
        return [], []

class DataSocket(BaseSocket):
    def draw_self(self, layout, node, text):
        if not (self.is_linked or self.is_output) and hasattr(self, "draw_property"):
            self.draw_property(layout, node, text)
        else:
            layout.label(text=text)

