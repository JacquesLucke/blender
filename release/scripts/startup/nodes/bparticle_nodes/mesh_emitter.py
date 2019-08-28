import bpy
from bpy.props import *
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class MeshEmitterNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_MeshEmitterNode"
    bl_label = "Mesh Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("rate", "Rate", "Float", default=10)
        builder.fixed_input("normal_velocity", "Normal Velocity", "Float", default=1)
        builder.fixed_input("emitter_velocity", "Emitter Velocity", "Float", default=0)
        builder.fixed_input("size", "Size", "Float", default=0.05)
        builder.fixed_input("density_vertex_group", "Density Group", "Text")
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.particle_effector_output("emitter", "Emitter")
