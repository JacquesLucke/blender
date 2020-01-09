import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class SimulationObjectNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulationObjectNode"
    bl_label = "State Object"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_output("state_object", "Object")

    def draw(self, layout):
        layout.prop(self, "name", text="")


class MergeSimulationObjectsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MergeSimulationObjectsNode"
    bl_label = "Merge"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects1", "Objects")
        builder.simulation_objects_input("objects2", "Objects")
        builder.simulation_objects_output("objects", "Objects")

class ApplySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ApplySolverNode"
    bl_label = "Apply Operation"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects", "Objects")
        builder.solver_input("operation", "Operation")
        builder.simulation_objects_output("objects", "Objects")

class ParticleSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleSolverNode"
    bl_label = "Particle Solver"

    def declaration(self, builder: NodeBuilder):
        builder.solver_output("solver", "Solver")

class RigidBodySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_RigidBodySolverNode"
    bl_label = "Rigid Body Solver"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("solver", "Objects")
        builder.simulation_objects_output("solver", "Objects")

class Attach3DGridNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Attach3DGridNode"
    bl_label = "Attach 3D Grid"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("name", "Name", "Text")
        builder.fixed_input("data_type", "Data Type", "Text", default="Float")
        builder.fixed_input("x", "X", "Integer", default=64)
        builder.fixed_input("y", "Y", "Integer", default=64)
        builder.fixed_input("z", "Z", "Integer", default=64)
        builder.solver_output("operation", "Operation")

class MultiSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MultiSolverNode"
    bl_label = "Multiple Operations"

    def declaration(self, builder: NodeBuilder):
        builder.solver_input("operation1", "1")
        builder.solver_input("operation2", "2")
        builder.solver_input("operation3", "3")
        builder.solver_output("solver", "Operation")

class Gravity1Node(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Gravity1Node"
    bl_label = "Gravity"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("solver", "Objects")
        builder.simulation_objects_output("solver", "Objects")


class SubstepsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SubstepsNode"
    bl_label = "Substeps"

    def declaration(self, builder: NodeBuilder):
        builder.solver_input("solver", "Operation")
        builder.fixed_input("substeps", "Substeps", "Integer", default=10)
        builder.solver_output("solver", "Operation")

class ConditionOperationNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ConditionOperationNode"
    bl_label = "Condition"

    def declaration(self, builder: NodeBuilder):
        builder.solver_input("solver", "Operation")
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.solver_output("solver", "Operation")

class AttachDynamicRigidBodyDataNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttachDynamicRigidBodyDataNode"
    bl_label = "Attach Dynamic RBD Data"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("solver", "Objects")
        builder.fixed_input("geometry", "Geometry", "Object")
        builder.simulation_objects_output("solver", "Objects")

class AttractForceNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttractForceNo"
    bl_label = "Attract Force"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("solver", "Objects")
        builder.fixed_input("point", "Point", "Vector")
        builder.simulation_objects_output("solver", "Objects")
