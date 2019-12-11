import bpy
from bpy.props import *
from .. base import FunctionNode
from .. node_builder import NodeBuilder

class ObjectMeshNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ObjectMeshNode"
    bl_label = "Object Mesh"

    def declaration(self, builder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_output("vertex_locations", "Vertex Locations", "Vector List")


class VertexInfo(bpy.types.Node, FunctionNode):
    bl_idname = "fn_VertexInfoNode"
    bl_label = "Vertex Info"

    def declaration(self, builder):
        builder.fixed_output("position", "Position", "Vector")


class ClosestLocationOnObjectNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ClosestLocationOnObjectNode"
    bl_label = "Closest Location on Object"

    use_list__object: NodeBuilder.VectorizedProperty()
    use_list__position: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input("object", "use_list__object", "Object", "Objects", "Object")
        builder.vectorized_input("position", "use_list__position", "Position", "Positions", "Vector")

        vectorize_props = ["use_list__object", "use_list__position"]
        builder.vectorized_output("closest_hook", vectorize_props, "Closest Hook", "Closest Hooks", "Surface Hook")
        builder.vectorized_output("closest_position", vectorize_props, "Closest Position", "Closest Positions", "Vector")
        builder.vectorized_output("closest_normal", vectorize_props, "Closest Normal", "Closest Normals", "Vector")


class GetPositionOnSurfaceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetPositionOnSurfaceNode"
    bl_label = "Get Position on Surface"

    use_list__surface_hook: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input("surface_hook", "use_list__surface_hook", "Surface Hook", "Surface Hooks", "Surface Hook")
        builder.vectorized_output("position", ["use_list__surface_hook"], "Position", "Positions", "Vector")


class GetNormalOnSurfaceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetNormalOnSurfaceNode"
    bl_label = "Get Normal on Surface"

    use_list__surface_hook: NodeBuilder.VectorizedProperty()

    def declaration(self, builder):
        builder.vectorized_input("surface_hook", "use_list__surface_hook", "Surface Hook", "Surface Hooks", "Surface Hook")
        builder.vectorized_output("normal", ["use_list__surface_hook"], "Normal", "Normals", "Vector")


class GetWeightOnSurfaceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetWeightOnSurfaceNode"
    bl_label = "Get Weight on Surface"

    use_list__surface_hook: NodeBuilder.VectorizedProperty()
    use_list__vertex_group_name: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input("surface_hook", "use_list__surface_hook", "Surface Hook", "Surface Hooks", "Surface Hook")
        builder.vectorized_input("vertex_group_name", "use_list__vertex_group_name", "Name", "Names", "Text", default="Group")
        builder.vectorized_output("weight", ["use_list__surface_hook", "use_list__vertex_group_name"], "Weight", "Weights", "Float")


class GetImageColorOnSurfaceNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_GetImageColorOnSurfaceNode"
    bl_label = "Get Image Color on Surface"

    use_list__surface_hook: NodeBuilder.VectorizedProperty()
    use_list__image: NodeBuilder.VectorizedProperty()

    def declaration(self, builder: NodeBuilder):
        builder.vectorized_input("surface_hook", "use_list__surface_hook", "Surface Hook", "Surface Hooks", "Surface Hook")
        builder.vectorized_input("image", "use_list__image", "Image", "Images", "Image")
        builder.vectorized_output("color", ["use_list__surface_hook", "use_list__image"], "Color", "Colors", "Color")
