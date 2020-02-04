import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class SimulationObjectNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulationObjectNode"
    bl_label = "State Object (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("state_object", "Object", "fn_SimulationObjectsSocket")

    def draw(self, layout):
        layout.prop(self, "name", text="")


class MergeSimulationObjectsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MergeSimulationObjectsNode"
    bl_label = "Merge (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects1", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_input("objects2", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

class ApplySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ApplySolverNode"
    bl_label = "Apply Operation (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_input("operation", "Operation", "fn_SimulationSolverSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

class ParticleSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleSolverNode"
    bl_label = "Particle Solver (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("solver", "Solver", "fn_SimulationSolverSocket")

class RigidBodySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_RigidBodySolverNode"
    bl_label = "Rigid Body Solver (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")

class Attach3DGridNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Attach3DGridNode"
    bl_label = "Attach 3D Grid (old)"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text")
        builder.fixed_input("data_type", "Data Type", "Text", default="Float")
        builder.fixed_input("x", "X", "Integer", default=64)
        builder.fixed_input("y", "Y", "Integer", default=64)
        builder.fixed_input("z", "Z", "Integer", default=64)
        builder.mockup_output("operation", "Operation", "fn_SimulationSolverSocket")

class MultiSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MultiSolverNode"
    bl_label = "Multiple Operations (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("operation1", "1", "fn_SimulationSolverSocket")
        builder.mockup_input("operation2", "2", "fn_SimulationSolverSocket")
        builder.mockup_input("operation3", "3", "fn_SimulationSolverSocket")
        builder.mockup_output("solver", "Operation", "fn_SimulationSolverSocket")

class Gravity1Node(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Gravity1Node"
    bl_label = "Gravity (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")


class SubstepsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SubstepsNode"
    bl_label = "Substeps (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Operation", "fn_SimulationSolverSocket")
        builder.fixed_input("substeps", "Substeps", "Integer", default=10)
        builder.mockup_output("solver", "Operation", "fn_SimulationSolverSocket")

class ConditionOperationNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ConditionOperationNode"
    bl_label = "Condition (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Operation", "fn_SimulationSolverSocket")
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.mockup_output("solver", "Operation", "fn_SimulationSolverSocket")

class AttachDynamicRigidBodyDataNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttachDynamicRigidBodyDataNode"
    bl_label = "Attach Dynamic RBD Data (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.fixed_input("geometry", "Geometry", "Object")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")

class AttractForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttractForceNode"
    bl_label = "Attract Force (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.fixed_input("point", "Point", "Vector")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")


class CrowdSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CrowdSolverNode"
    bl_label = "Crowd Solver (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

class AttachAgentBehaviorNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttachAgentBehaviorNode"
    bl_label = "Attach Agent Behavior (old)"

    behavior_tree: PointerProperty(type=bpy.types.NodeTree)

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

    def draw(self, layout):
        layout.prop(self, "behavior_tree", text="")


class OutputNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_OutputNode"
    bl_label = "Output (old)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")


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
    bl_label = "Particle System (1)"

    def declaration(self, builder: NodeBuilder):
        builder.influences_input("influences", "Influences")
        builder.mockup_output("system", "System", "fn_ParticleSystemsSocket")

class ParticleSystemNode3(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleSystemNode3"
    bl_label = "Particle System (2)"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("data", "Data", "fn_SimulationDataSocket")
        builder.influences_input("influences", "Influences")
        builder.mockup_output("system", "System", "fn_ParticleSystemsSocket")

class SimulationDataNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulationDataNode"
    bl_label = "Simulation Data"

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
        builder.mockup_input("data", "Data", "fn_SimulationDataSocket")
        builder.fixed_input("initial_geometry", "Initial Geometry", "Object")
        builder.influences_input("influences", "Influences")
        builder.mockup_output("cloth_object", "Cloth Object", "fn_ClothObjectSocket")

class MovingPointsForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MovingPointsForce"
    bl_label = "Moving Points Force"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("point_source", "Point Source", "fn_SimulationDataSocket")
        builder.fixed_input("strength", "Strength", "Float", default=1.0)
        builder.fixed_input("radius", "Radius", "Float", default=0.1)
        builder.influences_output("force", "Force")

class MeshCollisionEventNode2(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MeshCollisionEventNode2"
    bl_label = "Mesh Collision Event"

    execute__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("geometry_source", "Geometry Source", "fn_SimulationDataSocket")
        builder.execute_input("execute", "Execute on Event", "execute__prop")
        builder.influences_output("event", "Event")
