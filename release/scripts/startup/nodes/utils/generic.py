import string
import random

def iter_subclasses_recursive(cls):
    for sub in cls.__subclasses__():
        yield sub
        yield from iter_subclasses_recursive(sub)

def getattr_recursive(obj, name: str):
    if "." not in name and "[" not in name:
        return getattr(obj, name)
    else:
        # TODO: implement without eval
        return eval("obj." + name, globals(), locals())

def setattr_recursive(obj, name: str, value):
    if "." not in name and "[" not in name:
        setattr(obj, name, value)
    else:
        # TODO: implement without exec
        exec("obj." + name + " = value", globals(), locals())
