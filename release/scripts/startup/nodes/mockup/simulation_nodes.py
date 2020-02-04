import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class SimulationObjectNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulationObjectNode"
    bl_label = "State Object"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("state_object", "Object", "fn_SimulationObjectsSocket")

    def draw(self, layout):
        layout.prop(self, "name", text="")


class MergeSimulationObjectsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MergeSimulationObjectsNode"
    bl_label = "Merge"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects1", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_input("objects2", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

class ApplySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ApplySolverNode"
    bl_label = "Apply Operation"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_input("operation", "Operation", "fn_SimulationSolverSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

class ParticleSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleSolverNode"
    bl_label = "Particle Solver"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("solver", "Solver", "fn_SimulationSolverSocket")

class RigidBodySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_RigidBodySolverNode"
    bl_label = "Rigid Body Solver"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")

class Attach3DGridNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Attach3DGridNode"
    bl_label = "Attach 3D Grid"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text")
        builder.fixed_input("data_type", "Data Type", "Text", default="Float")
        builder.fixed_input("x", "X", "Integer", default=64)
        builder.fixed_input("y", "Y", "Integer", default=64)
        builder.fixed_input("z", "Z", "Integer", default=64)
        builder.mockup_output("operation", "Operation", "fn_SimulationSolverSocket")

class MultiSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MultiSolverNode"
    bl_label = "Multiple Operations"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("operation1", "1", "fn_SimulationSolverSocket")
        builder.mockup_input("operation2", "2", "fn_SimulationSolverSocket")
        builder.mockup_input("operation3", "3", "fn_SimulationSolverSocket")
        builder.mockup_output("solver", "Operation", "fn_SimulationSolverSocket")

class Gravity1Node(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Gravity1Node"
    bl_label = "Gravity"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")


class SubstepsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SubstepsNode"
    bl_label = "Substeps"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Operation", "fn_SimulationSolverSocket")
        builder.fixed_input("substeps", "Substeps", "Integer", default=10)
        builder.mockup_output("solver", "Operation", "fn_SimulationSolverSocket")

class ConditionOperationNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ConditionOperationNode"
    bl_label = "Condition"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Operation", "fn_SimulationSolverSocket")
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.mockup_output("solver", "Operation", "fn_SimulationSolverSocket")

class AttachDynamicRigidBodyDataNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttachDynamicRigidBodyDataNode"
    bl_label = "Attach Dynamic RBD Data"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.fixed_input("geometry", "Geometry", "Object")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")

class AttractForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttractForceNode"
    bl_label = "Attract Force"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("solver", "Objects", "fn_SimulationObjectsSocket")
        builder.fixed_input("point", "Point", "Vector")
        builder.mockup_output("solver", "Objects", "fn_SimulationObjectsSocket")


class CrowdSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CrowdSolverNode"
    bl_label = "Crowd Solver"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

class AttachAgentBehaviorNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttachAgentBehaviorNode"
    bl_label = "Attach Agent Behavior"

    behavior_tree: PointerProperty(type=bpy.types.NodeTree)

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")
        builder.mockup_output("objects", "Objects", "fn_SimulationObjectsSocket")

    def draw(self, layout):
        layout.prop(self, "behavior_tree", text="")


class OutputNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_OutputNode"
    bl_label = "Output"

    def declaration(self, builder: NodeBuilder):
        builder.mockup_input("objects", "Objects", "fn_SimulationObjectsSocket")
