import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

def create_variadic_math_node(data_type, idname, label):
    '''
    Should only be used for operations with associative property.
    '''

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

AddFloatsNode = create_variadic_math_node("Float", "fn_AddFloatsNode", "Add Floats")
MultiplyFloatsNode = create_variadic_math_node("Float", "fn_MultiplyFloatsNode", "Multiply Floats")
MinimumFloatsNode = create_variadic_math_node("Float", "fn_MinimumFloatsNode", "Minimum Floats")
MaximumFloatsNode = create_variadic_math_node("Float", "fn_MaximumFloatsNode", "Maximum Floats")


operation_items = [
    ("ADD",     "Add",          "", "", 1),
    ("SUB",     "Subtract",     "", "", 2),
    ("MULTIPLY", "Multiply",    "", "", 3),
    ("DIVIDE",  "Divide",       "", "", 4),
    None,
    ("POWER",   "Power",        "", "", 5),
    ("LOG",     "Logarithm",    "", "", 6),
    ("SQRT",    "Square Root",  "", "", 7),
    None,
    ("ABS",     "Absolute",     "", "", 8),
    ("MIN",     "Minimum",      "", "", 9),
    ("MAX",     "Maximum",      "", "", 10),
    ("MOD",     "Modulo",       "", "", 18),
    None,
    ("SIN",     "Sine",         "", "", 11),
    ("COS",     "Cosine",       "", "", 12),
    ("TAN",     "Tangent",      "", "", 13),
    ("ASIN",    "Arcsine",      "", "", 14),
    ("ACOS",    "Arccosine",    "", "", 15),
    ("ATAN",    "Arctangent",   "", "", 16),
    ("ATAN2",   "Arctan2",      "", "", 17),
    None,
    ("FRACT",   "Fract",        "", "", 19),
    ("CEIL",    "Ceil",         "", "", 20),
    ("FLOOR",   "Floor",        "", "", 21),
    ("ROUND",   "Round",        "", "", 22),
    ("SNAP",    "Snap",         "", "", 23),
]

single_value_operations = {
    "SQRT",
    "ABS",
    "SIN",
    "COS",
    "TAN",
    "ASIN",
    "ACOS",
    "ATAN",
    "FRACT",
    "CEIL",
    "FLOOR",
    "ROUND"
}

class FloatMathNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_FloatMathNode"
    bl_label = "Float Math"

    search_terms = (
        ("Subtract Floats", {"operation" : "SUB"}),
        ("Divide Floats", {"operation" : "DIVIDE"}),
    )

    operation: EnumProperty(
        name="Operation",
        items=operation_items,
        update=FunctionNode.sync_tree,
    )

    use_list__a: NodeBuilder.VectorizedProperty()
    use_list__b: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input(
            "a", "use_list__a",
            "A", "A", "Float")
        prop_names = ["use_list__a"]

        if self.operation not in single_value_operations:
            builder.vectorized_input(
                "b", "use_list__b",
                "B", "B", "Float")
            prop_names.append("use_list__b")


        builder.vectorized_output(
            "result", prop_names,
            "Result", "Result", "Float")

    def draw(self, layout):
        layout.prop(self, "operation", text="")
