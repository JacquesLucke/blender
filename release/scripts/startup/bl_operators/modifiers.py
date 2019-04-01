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

        from function_nodes.node_operators import new_function_tree
        tree = new_function_tree("Deformation Function", [
            ("Vector", "Old Position"),
            ("Integer", "Vertex Seed"),
            ("Float", "Control")
        ], [
            ("Vector", "New Position"),
        ])

        input_node = tree.get_input_node()
        output_node = tree.get_output_node()
        tree.new_link(input_node.outputs[0], output_node.inputs[0])

        mod.function_tree = tree
        return {'FINISHED'}

class NewPointGeneratorFunction(bpy.types.Operator, ModifierOperator):
    bl_idname = "fn.new_point_generator_function"
    bl_label = "New Point Generator Function"

    def execute(self, context):
        mod = self.get_modifier()
        if mod is None:
            return {'CANCELLED'}

        from function_nodes.node_operators import new_function_tree
        tree = new_function_tree("Point Generator", [
            ("Float", "Control 1"),
            ("Integer", "Control 2"),
        ], [
            ("Vector List", "Points"),
        ])

        mod.function_tree = tree
        return {'FINISHED'}

classes = (
    NewDeformationFunction,
    NewPointGeneratorFunction,
)