import bpy
from bpy.props import *
from .. base import SimulationNode, FunctionNode
from .. node_builder import NodeBuilder


class AgeReachedEventNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_AgeReachedEventNode"
    bl_label = "Age Reached Event"

    execute_on_event__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("age", "Age", "Float", default=3)
        builder.execute_input("execute_on_event", "Execute on Event", "execute_on_event__prop")

        builder.influences_output("event", "Event")


class MeshCollisionEventNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_MeshCollisionEventNode"
    bl_label = "Mesh Collision Event"

    execute_on_event__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.execute_input("execute_on_event", "Execute on Event", "execute_on_event__prop")

        builder.influences_output("event", "Event")


class CustomEventNode(bpy.types.Node, SimulationNode):
    bl_idname = "fn_CustomEventNode"
    bl_label = "Custom Event"

    execute_on_event__prop: NodeBuilder.ExecuteInputProperty()

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("condition", "Condition", "Boolean")
        builder.fixed_input("time_factor", "Time Factor", "Float", default=1.0)
        builder.execute_input("execute_on_event", "Execute on Event", "execute_on_event__prop")

        builder.influences_output("event", "Event")


class EventFilterEndTimeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_EventFilterEndTimeNode"
    bl_label = "Event Filter End Time"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_output("end_time", "End Time", "Float")
