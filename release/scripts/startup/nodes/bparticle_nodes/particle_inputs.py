import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class ParticleInfoNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleInfoNode"
    bl_label = "Particle Info"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("id", "ID", "Integer")
        builder.fixed_output("position", "Position", "Vector")
        builder.fixed_output("velocity", "Velocity", "Vector")
        builder.fixed_output("birth_time", "Birth Time", "Float")
        builder.fixed_output("emit_location", "Emit Location", "Surface Location")
