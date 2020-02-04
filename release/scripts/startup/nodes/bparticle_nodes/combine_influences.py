import bpy
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class CombineInfluencesNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CombineInfluencesNode"
    bl_label = "Combine Influences"

    def declaration(self, builder: NodeBuilder):
        builder.emitters_input("emitters", "Emitters")
        builder.emitters_output("emitters", "Emitters")
        builder.events_input("events", "Events")
        builder.events_output("events", "Events")
        builder.forces_input("forces", "Forces")
        builder.forces_output("forces", "Forces")
