import bpy
import bpy_test


class TEST_OT_refresh_expected(bpy.types.Operator):
    bl_idname = "test.refresh_expected"
    bl_label = "Refresh Expected Test Result"
    bl_description = "Record the state of the tested data so that it can be compared to later on"

    def execute(self, context):
        with bpy_test.run_in_generate_mode():
            bpy.ops.text.run_script()
        return {'FINISHED'}


class TEST_OT_clear_expected(bpy.types.Operator):
    bl_idname = "test.clear_expected"
    bl_label = "Clear Expected Test Results"
    bl_description = "Remove all data that has been generated when tests where refreshed"

    def execute(self, context):
        bpy_test.clear_generated_data()
        return {'FINISHED'}


class TEST_PT_tests(bpy.types.Panel):
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Text"
    bl_label = "Tests"

    def draw(self, context):
        layout = self.layout

        layout.operator("test.refresh_expected", text="Refresh Test", icon='FILE_REFRESH')
        layout.operator("test.clear_expected", text="Clear", icon='X')


classes = [
    TEST_OT_refresh_expected,
    TEST_OT_clear_expected,
    TEST_PT_tests,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
