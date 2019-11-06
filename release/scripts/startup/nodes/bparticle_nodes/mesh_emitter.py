import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder

class MeshEmitterNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MeshEmitterNode"
    bl_label = "Mesh Emitter"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    density_mode: EnumProperty(
        name="Density Mode",
        items=[
            ('UNIFORM', "Uniform", "", 'NONE', 0),
            ('VERTEX_WEIGHTS', "Vertex Weights", "", 'NONE', 1),
        ],
        update=SimulationNode.sync_tree,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("rate", "Rate", "Float", default=10)

        if self.density_mode == 'VERTEX_WEIGHTS':
            builder.fixed_input("density_vertex_group", "Density Group", "Text")

        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.influences_output("emitter", "Emitter")

    def draw(self, layout):
        layout.prop(self, "density_mode")
