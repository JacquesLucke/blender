import bpy
from bpy.props import *
from . base import BaseNode
from . utils.enum_items_cache import cache_enum_items

class NodeSearch(bpy.types.Operator):
    bl_idname = "fn.node_search"
    bl_label = "Node Search"
    bl_options = {'REGISTER', 'UNDO'}
    bl_property = "item"

    def get_search_items(self, context):
        items = []
        tree = context.space_data.edit_tree
        for node_cls in BaseNode.iter_final_subclasses():
            if node_cls.bl_label.startswith("Mockup") and not tree.show_mockups:
                continue
            for search_term, settings in node_cls.get_search_terms():
                item = encode_search_item(node_cls.bl_idname, search_term, settings)
                items.append(item)
        return items

    item: EnumProperty(items=cache_enum_items(get_search_items))

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname in ("FunctionTree", "BParticlesTree")
        except: return False

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        idname, settings = decode_search_item(self.item)
        op_settings = []
        for key, value in settings.items():
            item = {"name" : key, "value" : repr(value)}
            op_settings.append(item)
        bpy.ops.node.add_node('INVOKE_DEFAULT', type=idname, use_transform=True, settings=op_settings)
        return {'FINISHED'}


def encode_search_item(idname, search_term, settings):
    identifier = idname + ":" + repr(settings)
    return (identifier, search_term, "")

def decode_search_item(identifier):
    idname, settings_repr = identifier.split(":", 1)
    settings = eval(settings_repr)
    return idname, settings
