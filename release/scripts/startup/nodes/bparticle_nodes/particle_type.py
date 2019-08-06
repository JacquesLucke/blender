import bpy
from .. base import BParticlesNode
from .. node_builder import NodeBuilder

class ParticleTypeNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTypeNode"
    bl_label = "Particle Type"

    def declaration(self, builder: NodeBuilder):
        builder.background_color((0.8, 0.5, 0.4))

    def draw(self, layout):
        layout.prop(self, "name", text="", icon="MOD_PARTICLES")

        components = []
        for node in self.tree.nodes:
            if isinstance(node, BParticlesNode):
                if self.name in node.get_used_particle_type_names():
                    components.append(node)

        for node in components:
            layout.label(text=node.name)
