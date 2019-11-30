import bpy
from . function_tree import FunctionTree

def draw_menu(self, context):
    tree = context.space_data.node_tree
    if not isinstance(tree, FunctionTree):
        return

    layout = self.layout
    layout.operator_context = 'INVOKE_DEFAULT'

    layout.operator("fn.node_search", text="Search", icon='VIEWZOOM')
    layout.separator()
    layout.menu("FN_MT_function_nodes_menu", text="Function Nodes")
    layout.separator()
    insert_node(layout, "fn_ParticleSystemNode", "Particle System")
    layout.menu("BP_MT_influences_nodes_menu", text="Influences")
    layout.menu("BP_MT_action_nodes_menu", text="Actions")
    layout.menu("BP_MT_input_nodes_menu", text="Inputs")

class FunctionNodesMenu(bpy.types.Menu):
    bl_idname = "FN_MT_function_nodes_menu"
    bl_label = "Function Nodes Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'

        insert_node(layout, "fn_BooleanMathNode", "Boolean Math")
        insert_node(layout, "fn_CompareNode", "Compare")
        insert_node(layout, "fn_SwitchNode", "Switch")
        layout.separator()
        insert_node(layout, "fn_FloatRangeNode", "Float Range")
        layout.separator()
        insert_node(layout, "fn_CombineVectorNode", "Combine Vector")
        insert_node(layout, "fn_SeparateVectorNode", "Separate Vector")
        insert_node(layout, "fn_VectorDistanceNode", "Vector Distance")
        layout.separator()
        insert_node(layout, "fn_SeparateColorNode", "Separate Color")
        insert_node(layout, "fn_CombineColorNode", "Combine Color")
        layout.separator()
        insert_node(layout, "fn_GetListElementNode", "Get List Element")
        insert_node(layout, "fn_ListLengthNode", "List Length")
        insert_node(layout, "fn_PackListNode", "Pack List")
        layout.separator()
        insert_node(layout, "fn_ObjectMeshNode", "Object Mesh")
        insert_node(layout, "fn_ObjectTransformsNode", "Object Transforms")
        insert_node(layout, "fn_TextLengthNode", "Text Length")

class InfluencesNodesMenu(bpy.types.Menu):
    bl_idname = "BP_MT_influences_nodes_menu"
    bl_label = "Influences Nodes Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'

        insert_node(layout, "fn_CombineInfluencesNode", "Combine Influences")
        layout.separator()
        insert_node(layout, "fn_InitialGridEmitterNode", "Initial Grid Emitter")
        insert_node(layout, "fn_MeshEmitterNode", "Mesh Emitter")
        insert_node(layout, "fn_PointEmitterNode", "Point Emitter")
        layout.separator()
        insert_node(layout, "fn_AgeReachedEventNode", "Age Reached Event")
        insert_node(layout, "fn_MeshCollisionEventNode", "Mesh Collision Event")
        insert_node(layout, "fn_CustomEventNode", "Custom Event")
        layout.separator()
        insert_node(layout, "fn_ForceNode", "Force")
        layout.separator()
        insert_node(layout, "fn_SizeOverTimeNode", "Size Over Time")
        insert_node(layout, "fn_ParticleTrailsNode", "Trails")
        insert_node(layout, "fn_AlwaysExecuteNode", "Always Execute")


class ActionNodesMenu(bpy.types.Menu):
    bl_idname = "BP_MT_action_nodes_menu"
    bl_label = "Action Nodes Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'

        insert_node(layout, "fn_ChangeParticleColorNode", "Change Color")
        insert_node(layout, "fn_ChangeParticleVelocityNode", "Change Velocity")
        insert_node(layout, "fn_ChangeParticleSizeNode", "Change Size")
        insert_node(layout, "fn_ChangeParticlePositionNode", "Change Position")
        layout.separator()
        insert_node(layout, "fn_ExplodeParticleNode", "Explode Particle")
        insert_node(layout, "fn_KillParticleNode", "Kill Particle")
        insert_node(layout, "fn_ParticleConditionNode", "Condition")

class InputNodesMenu(bpy.types.Menu):
    bl_idname = "BP_MT_input_nodes_menu"
    bl_label = "Input Nodes Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'

        insert_node(layout, "fn_ParticleInfoNode", "Particle Info")


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
