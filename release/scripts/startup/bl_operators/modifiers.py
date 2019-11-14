import bpy
from bpy.props import (
    StringProperty,
)

class ModifierOperator:
    object_name: StringProperty()
    modifier_name: StringProperty()

    def get_modifier(self):
        ob = bpy.data.objects.get(self.object_name)
        if ob is None:
            return None
        return ob.modifiers.get(self.modifier_name)

class NewDeformationFunction(bpy.types.Operator, ModifierOperator):
    bl_idname = "fn.new_deformation_function"
    bl_label = "New Deformation Function"

    def execute(self, context):
        mod = self.get_modifier()
        if mod is None:
            return {'CANCELLED'}

        from nodes.node_operators import new_function_tree
        tree = new_function_tree("Deformation Function", [
            ("Vector", "Old Position"),
            ("Float", "Control 1"),
            ("Integer", "Control 2")
        ], [
            ("Vector", "New Position"),
        ])

        tree.new_link(
            tree.get_input_nodes()[0].outputs[0],
            tree.get_output_nodes()[0].inputs[0])

        mod.function_tree = tree
        return {'FINISHED'}

class NewPointGeneratorFunction(bpy.types.Operator, ModifierOperator):
    bl_idname = "fn.new_point_generator_function"
    bl_label = "New Point Generator Function"

    def execute(self, context):
        mod = self.get_modifier()
        if mod is None:
            return {'CANCELLED'}

        from nodes.node_operators import new_function_tree
        tree = new_function_tree("Point Generator", [
            ("Float", "Control 1"),
            ("Integer", "Control 2"),
        ], [
            ("Vector List", "Points"),
        ])

        mod.function_tree = tree
        return {'FINISHED'}

class NewParticleSimulationTree(bpy.types.Operator, ModifierOperator):
    bl_idname = "fn.new_particle_simulation_tree"
    bl_label = "New Particle Simulation Tree"

    def execute(self, context):
        mod = self.get_modifier()
        if mod is None:
            return {'CANCELLED'}

        tree = bpy.data.node_groups.new("Particle Simulation", "FunctionTree")

        type_node = tree.nodes.new("fn_ParticleSystemNode")

        emitter_node = tree.nodes.new("fn_InitialGridEmitterNode")
        emitter_node.location = (-250, 200)

        gravity_node = tree.nodes.new("fn_ForceNode")
        gravity_node.inputs[0].value = (0, 0, -1)
        gravity_node.location = (-250, -100)

        tree.links.new(emitter_node.outputs[0], type_node.inputs[0])
        tree.links.new(gravity_node.outputs[0], type_node.inputs[0])

        mod.node_tree = tree
        return {'FINISHED'}

classes = (
    NewDeformationFunction,
    NewPointGeneratorFunction,
    NewParticleSimulationTree,
)
