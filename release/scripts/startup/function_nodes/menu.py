import bpy
from . base import FunctionNodeTree

def draw_menu(self, context):
    tree = context.space_data.node_tree
    if not isinstance(tree, FunctionNodeTree):
        return

    layout = self.layout
    self.operator_context = "INVOKE_DEFAULT"

    insert_node(layout, "fn_AddFloatsNode", "Add Floats")
    insert_node(layout, "fn_CombineVectorNode", "Combine Vector")

def insert_node(layout, type, text, settings = {}, icon = "NONE"):
    operator = layout.operator("node.add_node", text = text, icon = icon)
    operator.type = type
    operator.use_transform = True
    for name, value in settings.items():
        item = operator.settings.add()
        item.name = name
        item.value = value
    return operator

bpy.types.NODE_MT_add.append(draw_menu)