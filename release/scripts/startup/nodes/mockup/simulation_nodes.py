import bpy
from bpy.props import *
from .. base import SimulationNode
from .. node_builder import NodeBuilder


class SimulationObjectNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_SimulationObjectNode"
    bl_label = "Simulation Object"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_output("object", "Object")

class AttachDataToSimulationObjectsNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AttachDataToSimulationObjectsNode"
    bl_label = "Attach Data to Object"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects", "Objects")
        builder.simulation_data_input("data", "Data")
        builder.simulation_objects_output("objects", "Objects")

class ParticleSolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_ParticleSolverNode"
    bl_label = "Particle Solver"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects", "Objects")
        builder.simulation_objects_output("objects", "Objects")

class RigidBodySolverNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_RigidBodySolverNode"
    bl_label = "Rigid Body Solver"

    def declaration(self, builder: NodeBuilder):
        builder.simulation_objects_input("objects", "Objects")
        builder.simulation_objects_output("objects", "Objects")
