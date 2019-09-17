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
    layout.menu("FN_MT_function_nodes_menu", text="Function Nodes")
    layout.separator()
    insert_node(layout, "bp_ParticleSystemNode", "Particle System")
    layout.menu("BP_MT_influences_nodes_menu", text="Influences")
    layout.menu("BP_MT_action_nodes_menu", text="Actions")
    layout.menu("BP_MT_input_nodes_menu", text="Inputs")

class FunctionNodesMenu(bpy.types.Menu):
    bl_idname = "FN_MT_function_nodes_menu"
    bl_label = "Function Nodes Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'

        insert_node(layout, "fn_FunctionInputNode", "Function Input")
        insert_node(layout, "fn_FunctionOutputNode", "Function Output")
        insert_node(layout, "fn_CallNode", "Call")
        layout.separator()
        insert_node(layout, "fn_BooleanMathNode", "Boolean Math")
        insert_node(layout, "fn_CompareNode", "Compare")
        insert_node(layout, "fn_SwitchNode", "Switch")
        layout.separator()
        insert_node(layout, "fn_FloatMathNode", "Float Math")
        insert_node(layout, "fn_FloatRangeNode", "Float Range")
        insert_node(layout, "fn_ClampNode", "Clamp")
        insert_node(layout, "fn_RandomNumberNode", "Random Number")
        insert_node(layout, "fn_MapRangeNode", "Map Range")
        layout.separator()
        insert_node(layout, "fn_VectorMathNode", "Vector Math")
        insert_node(layout, "fn_CombineVectorNode", "Combine Vector")
        insert_node(layout, "fn_SeparateVectorNode", "Separate Vector")
        insert_node(layout, "fn_VectorDistanceNode", "Vector Distance")
        layout.separator()
        insert_node(layout, "fn_SeparateColorNode", "Separate Color")
        insert_node(layout, "fn_CombineColorNode", "Combine Color")
        layout.separator()
        insert_node(layout, "fn_ConstantFalloffNode", "Constant Falloff")
        insert_node(layout, "fn_PointDistanceFalloffNode", "Point Distance Falloff")
        insert_node(layout, "fn_MeshDistanceFalloffNode", "Mesh Distance Falloff")
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

        insert_node(layout, "bp_CombineInfluencesNode", "Combine Influences")
        insert_node(layout, "bp_AlwaysExecuteNode", "Always Execute")
        insert_node(layout, "bp_AgeReachedEventNode", "Age Reached Event")
        insert_node(layout, "bp_MeshCollisionEventNode", "Mesh Collision Event")
        insert_node(layout, "bp_CustomEventNode", "Custom Event")

        insert_node(layout, "bp_TurbulenceForceNode", "Turbulence Force")
        insert_node(layout, "bp_GravityForceNode", "Gravity Force")
        insert_node(layout, "bp_DragForceNode", "Drag Force")
        insert_node(layout, "bp_MeshForceNode", "Mesh Force")

        insert_node(layout, "bp_InitialGridEmitterNode", "Initial Grid Emitter")
        insert_node(layout, "bp_MeshEmitterNode", "Mesh Emitter")
        insert_node(layout, "bp_PointEmitterNode", "Point Emitter")
        insert_node(layout, "bp_SizeOverTimeNode", "Size Over Time")
        insert_node(layout, "bp_ParticleTrailsNode", "Trails")


class ActionNodesMenu(bpy.types.Menu):
    bl_idname = "BP_MT_action_nodes_menu"
    bl_label = "Action Nodes Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'

        insert_node(layout, "bp_ChangeParticleColorNode", "Change Color")
        insert_node(layout, "bp_ChangeParticleVelocityNode", "Change Velocity")
        insert_node(layout, "bp_ChangeParticleSizeNode", "Change Size")
        insert_node(layout, "bp_ChangeParticlePositionNode", "Change Position")

        insert_node(layout, "bp_ParticleConditionNode", "Condition")
        insert_node(layout, "bp_ExplodeParticleNode", "Explode Particle")
        insert_node(layout, "bp_KillParticleNode", "Kill Particle")

class InputNodesMenu(bpy.types.Menu):
    bl_idname = "BP_MT_input_nodes_menu"
    bl_label = "Input Nodes Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'

        insert_node(layout, "bp_ParticleInfoNode", "Particle Info")
        insert_node(layout, "bp_SurfaceInfoNode", "Surface Info")
        insert_node(layout, "bp_SurfaceImageNode", "Image Colors")
        insert_node(layout, "bp_SurfaceWeightNode", "Vertex Weights")


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
