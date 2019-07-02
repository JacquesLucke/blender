import unittest

def register():
    loader = unittest.TestLoader()
    tests = loader.discover("function_nodes", pattern="test*")
    unittest.TextTestRunner(verbosity=1).run(tests)
