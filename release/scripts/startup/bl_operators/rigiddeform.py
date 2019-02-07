import bpy
from bpy.props import *

ANCHOR_GROUP_NAME = "Anchors"

class AddRigidDeformHandle(bpy.types.Operator):
    bl_idname = "object.add_rigid_deform_handle"
    bl_label = "Add Rigid Deform Handle"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        if ob is None: return False
        if ob.type != "MESH": return False
        if ob.mode != "EDIT": return False
        return True

    def execute(self, context):
        ob = context.active_object
        only_update_anchors = False

        if has_rigid_deform_modifier(ob):
            modifier = get_rigid_deform_modifier(ob)
            only_update_anchors = modifier.is_bind
        else:
            rigid_deform = ob.modifiers.new("Rigid Deform", "RIGID_DEFORM")
            ensure_anchor_group_exists(ob)
            rigid_deform.anchor_group_name = ANCHOR_GROUP_NAME


        new_hook_before_rigid_deform(context)
        add_selected_vertices_to_anchor_group(context)
        bind_rigid_deform_modifier(context, only_update_anchors)
        return {"FINISHED"}

def bind_rigid_deform_modifier(context, only_update):
    modifier = get_rigid_deform_modifier(context.active_object)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.rigiddeform_bind(modifier=modifier.name, only_update=only_update)

def new_hook_before_rigid_deform(context):
    hook = new_hook_modifier_from_selection(context)
    deform = get_rigid_deform_modifier(context.active_object)
    move_hook_before_rigid_deform(context, hook, deform)

def new_hook_modifier_from_selection(context):
    bpy.ops.object.hook_add_newob()
    return context.active_object.modifiers[-1]

def move_hook_before_rigid_deform(context, hook, deform):
    modifiers = list(context.active_object.modifiers)
    move_steps = modifiers.index(hook) - modifiers.index(deform)
    for _ in range(move_steps):
        bpy.ops.object.modifier_move_up(modifier=hook.name)

def has_rigid_deform_modifier(ob):
    return get_rigid_deform_modifier(ob) is not None

def get_rigid_deform_modifier(ob):
    for modifier in ob.modifiers:
        if modifier.type == "RIGID_DEFORM":
            return modifier
    return None

def add_selected_vertices_to_anchor_group(context):
    ob = context.active_object
    vertex_group = get_or_create_anchor_group(ob)
    ob.vertex_groups.active_index = vertex_group.index
    bpy.ops.object.vertex_group_assign()

def ensure_anchor_group_exists(ob):
    get_or_create_anchor_group(ob)

def get_or_create_anchor_group(ob):
    for vertex_group in ob.vertex_groups:
        if vertex_group.name == ANCHOR_GROUP_NAME:
            return vertex_group
    else:
        return ob.vertex_groups.new(name=ANCHOR_GROUP_NAME)


classes = (
    AddRigidDeformHandle,
)
