import bpy
from bpy.props import *
from . base import BaseNode

class NodeSearch(bpy.types.Operator):
    bl_idname = "fn.node_search"
    bl_label = "Node Search"
    bl_options = {'REGISTER', 'UNDO'}
    bl_property = "item"

    def get_search_items(self, context):
        items = []
        for cls in BaseNode.iter_final_subclasses():
            item = (cls.bl_idname, cls.bl_label, "")
            items.append(item)
        return items

    item: EnumProperty(items = get_search_items)

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname == "FunctionNodeTree"
        except: return False

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}

    def execute(self, context):
        idname = self.item
        bpy.ops.node.add_node('INVOKE_DEFAULT', type=idname, use_transform=True)
        return {'FINISHED'}
