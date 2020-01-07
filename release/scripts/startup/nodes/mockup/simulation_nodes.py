import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class SimulationObjectNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulationObjectNode"
    bl_label = "Simulation Object"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_output("object", "Object")

    def draw(self, layout):
        layout.prop(self, "name", text="")


class MergeSimulationObjectsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MergeSimulationObjectsNode"
    bl_label = "Merge Simulation Objects"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects1", "Objects")
        builder.simulation_objects_input("objects2", "Objects")
        builder.simulation_objects_output("objects", "Objects")

class ApplySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ApplySolverNode"
    bl_label = "Apply Solver"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects", "Objects")
        builder.solver_input("solver", "Solver")
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
        builder.solver_output("solver", "Solver")

class MultiSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MultiSolverNode"
    bl_label = "Multi Solver"

    def declaration(self, builder: NodeBuilder):
        builder.solver_input("solver1", "1")
        builder.solver_input("solver2", "2")
        builder.solver_input("solver3", "3")
        builder.solver_output("solver", "Solver")

class Gravity1Node(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Gravity1Node"
    bl_label = "Gravity 1"

    def declaration(self, builder: NodeBuilder):
        builder.solver_input("solver", "Solver")
        builder.solver_output("solver", "Solver")


class Gravity2Node(bpy.types.Node, SimulationNode):
    bl_idname = "fn_Gravity2Node"
    bl_label = "Gravity 2"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects", "Objects")
        builder.simulation_objects_output("objects", "Objects")

class SubstepsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SubstepsNode"
    bl_label = "Substeps"

    def declaration(self, builder: NodeBuilder):
        builder.solver_input("solver", "Solver")
        builder.fixed_input("substeps", "Substeps", "Integer", default=10)
        builder.solver_output("solver", "Solver")
