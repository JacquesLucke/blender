import unittest
from . inferencer import Inferencer, ConflictingTypesError

class TestInferencer(unittest.TestCase):
    def setUp(self):
        self.inferencer = Inferencer()

    def test_single_equality(self):
        self.inferencer.insert_equality_constraint((1, 2))
        self.inferencer.insert_final_type(1, "Float")

        self.assertTrue(self.inferencer.inference())

        self.assertEqual(self.inferencer.get_final_type(1), "Float")
        self.assertEqual(self.inferencer.get_final_type(2), "Float")

    def test_multiple_equality(self):
        self.inferencer.insert_equality_constraint((1, 2, 3))
        self.inferencer.insert_equality_constraint((3, 4, 5))
        self.inferencer.insert_final_type(4, "Integer")

        self.assertTrue(self.inferencer.inference())
        self.assertEqual(self.inferencer.get_final_type(1), "Integer")
        self.assertEqual(self.inferencer.get_final_type(3), "Integer")
        self.assertEqual(self.inferencer.get_final_type(5), "Integer")

    def test_find_base(self):
        self.inferencer.insert_list_constraint((1, ), (2,))
        self.inferencer.insert_final_type(1, "Float List")

        self.assertTrue(self.inferencer.inference())
        self.assertEqual(self.inferencer.get_final_type(2), "Float")

    def test_find_list(self):
        self.inferencer.insert_list_constraint((1, ), (2, ))
        self.inferencer.insert_final_type(2, "Vector")

        self.assertTrue(self.inferencer.inference())
        self.assertEqual(self.inferencer.get_final_type(1), "Vector List")

    def test_invalid_equality(self):
        self.inferencer.insert_equality_constraint((1, 2))
        self.inferencer.insert_final_type(1, "Float")
        self.inferencer.insert_final_type(2, "Integer")

        with self.assertRaises(ConflictingTypesError):
            self.inferencer.inference()

    def test_cannot_finalize_all(self):
        self.inferencer.insert_list_constraint((1, 2))
        self.assertFalse(self.inferencer.inference())
