import bpy

class FunctionNodeTree(bpy.types.NodeTree):
    bl_idname = "FunctionNodeTree"
    bl_icon = "MOD_DATA_TRANSFER"
    bl_label = "Function Nodes"

bpy.utils.register_class(FunctionNodeTree)


class FunctionNode:
    def init(self, context):
        inputs, outputs = self.get_sockets()
        for idname, name in inputs:
            self.inputs.new(idname, name)
        for idname, name in outputs:
            self.outputs.new(idname, name)

    def get_sockets():
        return [], []

class DataSocket:
    color = (0, 0, 0, 0)

    def draw_color(self, context, node):
        return self.color

    def draw(self, context, layout, node, text):
        if not (self.is_linked or self.is_output) and hasattr(self, "draw_property"):
            self.draw_property(layout, text, node)
        else:
            layout.label(text=text)

