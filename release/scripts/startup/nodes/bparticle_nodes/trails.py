import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class TrailEmitterNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleTrailsNode"
    bl_label = "Trails Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("points", "Points", "fn_GeometrySocket")
        builder.fixed_input("rate", "Rate", "Float", default=20)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.mockup_output("emitter", "Emitter", "fn_EmittersSocket")
