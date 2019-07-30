import bpy
from . function_tree import FunctionTree
from . bparticles_tree import BParticlesTree

def draw_menu(self, context):
    tree = context.space_data.node_tree
    if not isinstance(tree, (FunctionTree, BParticlesTree)):
        return

    layout = self.layout
    layout.operator_context = 'INVOKE_DEFAULT'

    layout.operator("fn.node_search", text="Search", icon='VIEWZOOM')
    layout.separator()
    insert_node(layout, "fn_FunctionInputNode", "Function Input")
    insert_node(layout, "fn_FunctionOutputNode", "Function Output")
    layout.separator()
    insert_node(layout, "fn_FloatMathNode", "Float Math")
    insert_node(layout, "fn_CombineVectorNode", "Combine Vector")
    insert_node(layout, "fn_SeparateVectorNode", "Separate Vector")
    insert_node(layout, "fn_VectorDistanceNode", "Vector Distance")
    insert_node(layout, "fn_ClampNode", "Clamp")
    insert_node(layout, "fn_RandomNumberNode", "Random Number")
    insert_node(layout, "fn_MapRangeNode", "Map Range")
    insert_node(layout, "fn_ObjectTransformsNode", "Object Transforms")

def insert_node(layout, type, text, settings = {}, icon = "NONE"):
    operator = layout.operator("node.add_node", text = text, icon = icon)
    operator.type = type
    operator.use_transform = True
    for name, value in settings.items():
        item = operator.settings.add()
        item.name = name
        item.value = value
    return operator


def register():
    bpy.types.NODE_MT_add.append(draw_menu)
