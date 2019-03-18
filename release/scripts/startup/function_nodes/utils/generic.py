import string
import random

def iter_subclasses_recursive(cls):
    for sub in cls.__subclasses__():
        yield sub
        yield from iter_subclasses_recursive(sub)