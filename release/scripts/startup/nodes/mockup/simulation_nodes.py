import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class SimulateNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulateNode"
    bl_label = "Simulate"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver1", "Solvers", "fn_ExecuteSolverSocket")
        builder.mockup_input("solver2", "Then", "fn_ExecuteSolverSocket")
        builder.mockup_input("solver3", "Then", "fn_ExecuteSolverSocket")
        builder.mockup_input("solver4", "Then", "fn_ExecuteSolverSocket")
        builder.mockup_input("solver5", "Then", "fn_ExecuteSolverSocket")

class ParticleSolverNode2(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleSolverNode2"
    bl_label = "Particle Solver"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("systems", "Systems", "fn_ParticleSystemsSocket")
        builder.mockup_output("solver", "Solver", "fn_ExecuteSolverSocket")

class ParticleSystemNode2(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleSystemNode2"
    bl_label = "Particle System"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text", display_icon="FILE_3D")

        builder.mockup_input("emitters", "Emitters", "fn_EmittersSocket")
        builder.mockup_input("events", "Events", "fn_EventsSocket")
        builder.mockup_input("forces", "Forces", "fn_ForcesSocket")
        builder.mockup_output("system", "System", "fn_ParticleSystemsSocket")

class SimulationDataNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulationDataNode"
    bl_label = "Simulation Data (old)"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text", display_name=False, default="Name")
        builder.mockup_output("data", "Data", "fn_SimulationDataSocket")

class ClothSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ClothSolverNode"
    bl_label = "Cloth Solver"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("cloth_objects", "Cloth Objects", "fn_ClothObjectSocket")
        builder.mockup_output("solver", "Solver", "fn_ExecuteSolverSocket")

class ClothObjectNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ClothObjectNode"
    bl_label = "Cloth Object"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text", display_icon="FILE_3D")
        builder.mockup_input("initial_geometry", "Initial Geometry", "fn_GeometrySocket")
        builder.mockup_input("forces", "Forces", "fn_ForcesSocket")
        builder.mockup_output("cloth_object", "Cloth Object", "fn_ClothObjectSocket")

class MovingPointsForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MovingPointsForce"
    bl_label = "Moving Points Force"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("point_source", "Point Source", "fn_GeometrySocket")
        builder.fixed_input("strength", "Strength", "Float", default=1.0)
        builder.fixed_input("radius", "Radius", "Float", default=0.1)
        builder.mockup_output("force", "Force", "fn_ForcesSocket")

class MeshCollisionEventNode2(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MeshCollisionEventNode2"
    bl_label = "Mesh Collision Event"

    execute__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry", "Geometry", "fn_GeometrySocket")
        builder.execute_input("execute", "Execute on Event", "execute__prop")
        builder.mockup_output("event", "Event", "fn_EventsSocket")

class GetGeometryNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_GetGeometryNode"
    bl_label = "Get Geometry"

    mode = EnumProperty(
        items=[
            ("DATA_BLOCK", "Object Data Block", ""),
            ("DYNAMIC_GEOMETRY", "Dynamic Geometry", ""),
        ],
        update=SimulationNode.sync_tree
    )

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")
        if self.mode == "DATA_BLOCK":
            builder.fixed_input("object", "Object", "Object")
        elif self.mode == "DYNAMIC_GEOMETRY":
            builder.fixed_input("name", "Name", "Text", display_icon="FILE_3D")

    def draw(self, layout):
        layout.prop(self, "mode", text="")
