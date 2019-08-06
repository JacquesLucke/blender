import bpy
from bpy.props import *
from . base import BaseNode
from . utils.enum_items_cache import cache_enum_items

class ParticleTypeSearch(bpy.types.Operator):
    bl_idname = "bp.particle_type_search"
    bl_label = "Particle Type Search"
    bl_options = {'REGISTER', 'UNDO', 'INTERNAL'}
    bl_property = "item"

    def get_search_items(self, context):
        items = []
        tree = bpy.data.node_groups[self.tree_name]
        for node in tree.nodes:
            if node.bl_idname == "bp_ParticleTypeNode":
                item = (str(node.node_identifier), node.name, "")
                items.append(item)
        items.append(("NONE", "None", ""))
        return items

    item: EnumProperty(items=cache_enum_items(get_search_items))

    tree_name: StringProperty()
    node_name: StringProperty()
    prop_name: StringProperty()

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        item = self.item
        if item == "NONE":
            self.set_type(0)
        else:
            self.set_type(int(item))
        context.area.tag_redraw()
        return {'FINISHED'}

    def set_type(self, type_id):
        tree = bpy.data.node_groups[self.tree_name]
        node = tree.nodes[self.node_name]
        setattr(node, self.prop_name, type_id)
