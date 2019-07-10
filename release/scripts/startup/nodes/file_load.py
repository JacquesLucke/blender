import bpy
from bpy.app.handlers import persistent

@persistent
def file_load_handler(dummy):
    from . sync import sync_trees_and_dependent_trees
    node_trees = set(bpy.data.node_groups)
    sync_trees_and_dependent_trees(node_trees)

def register():
    bpy.app.handlers.load_post.append(file_load_handler)

def unregister():
    bpy.app.handlers.load_post.remove(file_load_handler)
