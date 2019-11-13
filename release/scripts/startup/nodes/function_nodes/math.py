import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

def create_variadic_math_node(data_type, idname, label):

    class MathNode(bpy.types.Node, FunctionNode):
        bl_idname = idname
        bl_label = label

        variadic: NodeBuilder.BaseListVariadicProperty()

        def declaration(self, builder: NodeBuilder):
            builder.base_list_variadic_input("inputs", "variadic", data_type)

            if NodeBuilder.BaseListVariadicPropertyHasList(self.variadic):
                builder.fixed_output("result", "Result", data_type + " List")
            else:
                builder.fixed_output("result", "Result", data_type)

    return MathNode

def create_two_inputs_math_node(data_type, idname, label):
    return create_two_inputs_other_output_math_node(data_type, data_type, idname, label)

def create_single_input_math_node(data_type, idname, label):

    class MathNode(bpy.types.Node, FunctionNode):
        bl_idname = idname
        bl_label = label

        use_list: NodeBuilder.VectorizedProperty()

        def declaration(self, builder: NodeBuilder):
            builder.vectorized_input("input", "use_list", "Value", "Values", data_type)
            builder.vectorized_output("output", ["use_list"], "Result", "Result", data_type)

    return MathNode

def create_two_inputs_other_output_math_node(input_type, output_type, idname, label):

    class MathNode(bpy.types.Node, FunctionNode):
        bl_idname = idname
        bl_label = label

        use_list__a: NodeBuilder.VectorizedProperty()
        use_list__b: NodeBuilder.VectorizedProperty()

        def declaration(self, builder: NodeBuilder):
            builder.vectorized_input("a", "use_list__a", "A", "A", input_type)
            builder.vectorized_input("b", "use_list__b", "B", "B", input_type)
            builder.vectorized_output("result", ["use_list__a", "use_list__b"], "Result", "Result", output_type)

    return MathNode

AddFloatsNode = create_variadic_math_node("Float", "fn_AddFloatsNode", "Add Floats")
MultiplyFloatsNode = create_variadic_math_node("Float", "fn_MultiplyFloatsNode", "Multiply Floats")
MinimumFloatsNode = create_variadic_math_node("Float", "fn_MinimumFloatsNode", "Minimum Floats")
MaximumFloatsNode = create_variadic_math_node("Float", "fn_MaximumFloatsNode", "Maximum Floats")

SubtractFloatsNode = create_two_inputs_math_node("Float", "fn_SubtractFloatsNode", "Subtract Floats")
DivideFloatsNode = create_two_inputs_math_node("Float", "fn_DivideFloatsNode", "Divide Floats")
PowerFloatsNode = create_two_inputs_math_node("Float", "fn_PowerFloatsNode", "Power Floats")

SqrtFloatNode = create_single_input_math_node("Float", "fn_SqrtFloatNode", "Sqrt Float")
AbsFloatNode = create_single_input_math_node("Float", "fn_AbsoluteFloatNode", "Absolute Float")
SineFloatNode = create_single_input_math_node("Float", "fn_SineFloatNode", "Sine")
CosineFloatNode = create_single_input_math_node("Float", "fn_CosineFloatNode", "Cosine")

AddVectorsNode = create_variadic_math_node("Vector", "fn_AddVectorsNode", "Add Vectors")
SubtractVectorsNode = create_two_inputs_math_node("Vector", "fn_SubtractVectorsNode", "Subtract Vectors")
MultiplyVectorsNode = create_variadic_math_node("Vector", "fn_MultiplyVectorsNode", "Multiply Vectors")
DivideVectorsNode = create_two_inputs_math_node("Vector", "fn_DivideVectorsNode", "Divide Vectors")

VectorCrossProductNode = create_two_inputs_math_node("Vector", "fn_VectorCrossProductNode", "Cross Product")
VectorReflectNode = create_two_inputs_math_node("Vector", "fn_ReflectVectorNode", "Reflect Vector")
VectorProjectNode = create_two_inputs_math_node("Vector", "fn_ProjectVectorNode", "Project Vector")
VectorDotProductNode = create_two_inputs_other_output_math_node("Vector", "Float", "fn_VectorDotProductNode", "Dot Product") 
VectorDistanceNode = create_two_inputs_other_output_math_node("Vector", "Float", "fn_VectorDistanceNode", "Vector Distance")

BooleanAndNode = create_variadic_math_node("Boolean", "fn_BooleanAndNode", "And")
BooleanOrNode = create_variadic_math_node("Boolean", "fn_BooleanOrNode", "Or")
BooleanNotNode = create_single_input_math_node("Boolean", "fn_BooleanNotNode", "Not")


class MapRangeNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_MapRangeNode"
    bl_label = "Map Range"

    clamp: BoolProperty(
        name="Clamp",
        default=True,
    )

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("value", "Value", "Float")
        builder.fixed_input("from_min", "From Min", "Float", default=0)
        builder.fixed_input("from_max", "From Max", "Float", default=1)
        builder.fixed_input("to_min", "To Min", "Float", default=0)
        builder.fixed_input("to_max", "To Max", "Float", default=1)
        builder.fixed_output("value", "Value", "Float")

    def draw(self, layout):
        layout.prop(self, "clamp")

class FloatClampNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_FloatClampNode"
    bl_label = "Clamp"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("value", "Value", "Float")
        builder.fixed_input("min", "Min", "Float", default=0)
        builder.fixed_input("max", "Max", "Float", default=1)
        builder.fixed_output("value", "Value", "Float")
