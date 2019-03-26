import bpy
from . function_tree import FunctionTree

class TreePanel(bpy.types.Panel):
    bl_idname = "FN_PT_tree_panel"
    bl_label = "Functions Tree"
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Node"

    @classmethod
    def poll(self, context):
        try: return isinstance(context.space_data.edit_tree, FunctionTree)
        except: return False

    def draw(self, context):
        layout = self.layout
