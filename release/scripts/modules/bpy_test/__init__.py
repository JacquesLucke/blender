import bpy
import enum
from typing import Optional

class TestMode(enum.Enum):
    GENERATE = enum.auto()
    TEST = enum.auto()

global_test_mode = TestMode.TEST

test_prefix = "_bpy_test_"

def assert_eq_list(a, b):
    assert list(a) == list(b)

class Tester:
    def __init__(self, mode: Optional[TestMode] = None):
        self.mode = mode if mode is not None else global_test_mode
        print(self.mode)

    def assert_object_location(self, object_name: str):
        ob = bpy.data.objects[object_name]
        prop_name = test_prefix + "location"

        if self.mode == TestMode.GENERATE:
            ob[prop_name] = ob.location
        else:
            stored_location = ob[prop_name]
            assert_eq_list(ob.location, stored_location)

def clear_generated_data():
    for ob in bpy.data.objects:
        names_to_remove = [name for name in ob.keys() if name.startswith(test_prefix)]
        for name in names_to_remove:
            del ob[name]
