import bpy
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class CollisionObjectNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CollisionObjectNode"
    bl_label = "Collision Object"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object", display_name=False)
        builder.influences_output("collider", "Collider")
