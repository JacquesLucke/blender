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
        builder.mockup_input("colliders", "Colliders", "fn_CollidersSocket")
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
        builder.mockup_input("colliders", "Colliders", "fn_CollidersSocket")
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
            ("OBJECT", "Object", ""),
            ("SIMULATION_OBJECT", "Simulation Object", ""),
        ],
        update=SimulationNode.sync_tree
    )

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("geometry", "Geometry", "fn_GeometrySocket")
        if self.mode == "OBJECT":
            builder.fixed_input("object", "Object", "Object")
        elif self.mode == "SIMULATION_OBJECT":
            builder.fixed_input("name", "Name", "Text", display_icon="FILE_3D")

    def draw(self, layout):
        layout.prop(self, "mode", text="")

class TrailEmitterNode1(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleTrailsNode1"
    bl_label = "Trails Emitter (1)"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("points", "Points", "fn_GeometrySocket")
        builder.fixed_input("rate", "Rate", "Float", default=20)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.mockup_output("emitter", "Emitter", "fn_EmittersSocket")

class TrailEmitterNode2(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleTrailsNode2"
    bl_label = "Trails Emitter (2)"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("rate", "Rate", "Float", default=20)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.mockup_output("event", "Event", "fn_EventsSocket")
        builder.mockup_output("emitter", "Emitter", "fn_EmittersSocket")

class TrailEmitterNode3(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleTrailsNode3"
    bl_label = "Trails Emitter (3)"

    execute_on_birth__prop: NodeBuilder.ExecuteInputProperty()

    source_name: StringProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("rate", "Rate", "Float", default=20)
        builder.execute_input("execute_on_birth", "Execute on Birth", "execute_on_birth__prop")
        builder.mockup_output("emitter", "Emitter", "fn_EmittersSocket")

    def draw(self, layout):
        layout.prop(self, "source_name", text="Source", icon="FILE_3D")

class StaticColliderNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_StaticColliderObjectNode"
    bl_label = "Static Collider"

    mode: EnumProperty(
        items=[
            ("OBJECT", "Object", ""),
            ("COLLECTION", "Collection", ""),
            ("SIMULATION_OBJECT", "Simulation Object", ""),
        ],
        update=SimulationNode.sync_tree
    )

    collection: PointerProperty(type=bpy.types.Collection)

    use_settings_from_object: BoolProperty(default=True, update=SimulationNode.sync_tree)

    def declaration(self, builder: NodeBuilder):
        if self.mode == "OBJECT":
            builder.fixed_input("object", "Object", "Object")
        elif self.mode == "SIMULATION_OBJECT":
            builder.mockup_input("geometry", "Geometry", "fn_GeometrySocket")

        if self.mode == "SIMULATION_OBJECT" or not self.use_settings_from_object:
            builder.fixed_input("stickiness", "Stickiness", "Float")
            builder.fixed_input("damping", "Damping", "Float")
            builder.fixed_input("friction", "Friction", "Float")
        builder.mockup_output("collider", "Collider", "fn_CollidersSocket")

    def draw(self, layout):
        layout.prop(self, "mode", text="")
        if self.mode == "COLLECTION":
            layout.prop(self, "collection", text="")
        layout.prop(self, "use_settings_from_object", text="Use Settings from Object")
