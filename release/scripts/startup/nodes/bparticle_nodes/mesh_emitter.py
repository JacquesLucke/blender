import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshEmitterNode"
    bl_label = "Mesh Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    density_mode: EnumProperty(
        name="Density Mode",
        items=[
            ('UNIFORM', "Uniform", "", 'NONE', 0),
            ('VERTEX_WEIGHTS', "Vertex Weights", "", 'NONE', 1),
            ('FALLOFF', "Falloff", "", 'NONE', 2),
        ],
        update=BParticlesNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("rate", "Rate", "Float", default=10)

        if self.density_mode == 'VERTEX_WEIGHTS':
            builder.fixed_input("density_vertex_group", "Density Group", "Text")
        elif self.density_mode == 'FALLOFF':
            builder.fixed_input("density_falloff", "Density Falloff", "Falloff")

        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.influences_output("emitter", "Emitter")

    def draw(self, layout):
        layout.prop(self, "density_mode")
