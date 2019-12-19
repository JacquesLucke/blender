import bpy
from bpy.props import *
from pathlib import Path
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
            for search_term, settings in node_cls.get_search_terms():
                item = encode_search_item(("BUILTIN", node_cls.bl_idname, settings), search_term)
                items.append(item)

        current_tree = context.space_data.node_tree
        for tree in current_tree.find_callable_trees():
            item = encode_search_item(("EXISTING_GROUP", tree.name), "(G) " + tree.name)
            items.append(item)

        local_group_names = set(tree.name for tree in bpy.data.node_groups)
        nodelibdir = Path(context.preferences.filepaths.nodelib_directory)
        for path in nodelibdir.glob("**/*"):
            if not path.is_file():
                continue
            with bpy.data.libraries.load(str(path)) as (data_from, data_to):
                for group_name in data_from.node_groups:
                    if group_name not in local_group_names:
                        item = encode_search_item(("LIB_GROUP", str(path), group_name), "(G) " + group_name)
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

        item_data = decode_search_item(self.item)
        item_type = item_data[0]

        if item_type == "BUILTIN":
            idname, settings = item_data[1:]
            bpy.ops.node.add_node('INVOKE_DEFAULT', type=idname)
            new_node = context.active_node
            for key, value in settings.items():
                setattr(new_node, key, value)
        elif item_type == "EXISTING_GROUP":
            group_name = item_data[1]
            bpy.ops.node.add_node('INVOKE_DEFAULT', type="fn_GroupNode")
            new_node = context.active_node
            new_node.node_group = bpy.data.node_groups[group_name]
        elif item_type == "LIB_GROUP":
            path, group_name = item_data[1:]
            bpy.ops.node.add_node('INVOKE_DEFAULT', type="fn_GroupNode")
            new_node = context.active_node
            with bpy.data.libraries.load(path, link=True) as (data_from, data_to):
                data_to.node_groups = [group_name]
            new_node.node_group = bpy.data.node_groups[group_name]

        bpy.ops.node.translate_attach("INVOKE_DEFAULT")
        return {'FINISHED'}


def encode_search_item(data, search_term):
    identifier = repr(data)
    return (identifier, search_term, "")

def decode_search_item(identifier):
    return eval(identifier)
