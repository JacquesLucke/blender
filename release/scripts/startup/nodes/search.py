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

        current_tree = context.space_data.node_tree
        for tree in current_tree.find_callable_trees():
            name = "(G) " + tree.name
            item = encode_search_item(name, name, {})
            items.append(item)

        sorted_items = list(sorted(items, key=lambda item: item[1]))
        return sorted_items

    item: EnumProperty(items=cache_enum_items(get_search_items))

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname == "FunctionTree"
        except: return False

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        tree = context.space_data.node_tree
        for node in tree.nodes:
            node.select = False

        idname, settings = decode_search_item(self.item)
        if idname.startswith("(G) "):
            group_name = idname[len("(G) "):]
            idname = "fn_GroupNode"
            node_group = bpy.data.node_groups[group_name]
            settings = {"node_group" : node_group}

        bpy.ops.node.add_node('INVOKE_DEFAULT', type=idname)
        new_node = context.active_node
        new_node.select = True
        for key, value in settings.items():
            setattr(new_node, key, value)

        bpy.ops.node.translate_attach("INVOKE_DEFAULT")
        return {'FINISHED'}


def encode_search_item(idname, search_term, settings):
    identifier = idname + ":" + repr(settings)
    return (identifier, search_term, "")

def decode_search_item(identifier):
    idname, settings_repr = identifier.split(":", 1)
    settings = eval(settings_repr)
    return idname, settings
