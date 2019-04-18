import itertools
from collections import namedtuple

'''
Type Rules
==========

A -> B means, Type A can be converted to type B implicitely.
A -!> B means, Type A cannot be converted to type B implicitely.
A_List is the type that contains a list of elements of type A

Iff T1 -> T2, then T1_List -> T2_List.
T -> T_List.
T_List -!> T.

Types always come in pairs: T and T_List.
There are no lists of lists.

<
Every group of implicitely convertable types, must define an order.
This order specifies which type should be worked with, when multiple types come together.
E.g. when adding a Float and an Integer, a float addition is performed.
> not yet
'''

# Type Info Container
#####################################

ImplicitConversion = namedtuple("ImplicitConversion", ("from_type", "to_type"))

class DataTypesInfo:
    def __init__(self):
        self.data_types = set()
        self.builder_by_data_type = dict()
        self.list_by_base = dict()
        self.base_by_list = dict()
        self.implicit_conversions = set()


    # Insert New Information
    #############################

    def insert_data_type(self, base_type_name, base_builder, list_builder):
        base_type = base_type_name
        list_type = base_type_name + " List"

        assert base_type not in self.data_types
        assert list_type not in self.data_types
        assert isinstance(base_builder, DataSocketBuilder)
        assert isinstance(list_builder, DataSocketBuilder)

        self.data_types.add(base_type)
        self.data_types.add(list_type)
        self.list_by_base[base_type] = list_type
        self.base_by_list[list_type] = base_type
        self.builder_by_data_type[base_type] = base_builder
        self.builder_by_data_type[list_type] = list_builder

        list_conversion = ImplicitConversion(base_type, list_type)
        self.implicit_conversions.add(list_conversion)

    def insert_implicit_conversion(self, from_type, to_type):
        assert self.is_data_type(from_type)
        assert self.is_data_type(to_type)
        assert self.is_base(from_type)
        assert self.is_base(to_type)

        base_conversion = ImplicitConversion(from_type, to_type)
        assert base_conversion not in self.implicit_conversions
        self.implicit_conversions.add(base_conversion)

        list_conversion = ImplicitConversion(
            self.to_list(from_type), self.to_list(to_type))
        assert list_conversion not in self.implicit_conversions
        self.implicit_conversions.add(list_conversion)


    # Query Information
    ##########################

    def is_data_type(self, data_type):
        return data_type in self.data_types

    def is_base(self, data_type):
        return data_type in self.list_by_base

    def is_list(self, data_type):
        return data_type in self.base_by_list

    def to_list(self, data_type):
        assert self.is_base(data_type)
        return self.list_by_base[data_type]

    def to_base(self, data_type):
        assert self.is_list(data_type)
        return self.base_by_list[data_type]

    def to_builder(self, data_type):
        assert self.is_data_type(data_type)
        return self.builder_by_data_type[data_type]

    def build(self, data_type, node_sockets, name, identifier):
        builder = self.to_builder(data_type)
        socket = builder.build(node_sockets, name, identifier)
        socket.data_type = data_type
        return socket

    def get_data_type_items(self):
        items = []
        for data_type in self.data_types:
            items.append((data_type, data_type, ""))
        return items

    def get_base_type_items(self):
        items = []
        for data_type in self.iter_base_types():
            items.append((data_type, data_type, ""))
        return items

    def get_data_type_items_cb(self):
        def callback(_1, _2):
            return self.get_data_type_items()
        return callback

    def get_socket_color(self, data_type):
        builder = self.to_builder(data_type)
        return builder.get_color()

    def is_link_allowed(self, from_type, to_type):
        assert self.is_data_type(from_type)
        assert self.is_data_type(to_type)

        if from_type == to_type:
            return True
        else:
            return self.is_implicitly_convertable(from_type, to_type)

    def is_implicitly_convertable(self, from_type, to_type):
        return ImplicitConversion(from_type, to_type) in self.implicit_conversions

    def iter_list_types(self):
        yield from self.base_by_list.keys()

    def iter_base_types(self):
        yield from self.list_by_base.keys()


# Data Socket Builders
##################################3

class DataSocketBuilder:
    def build(self, node_sockets, name):
        raise NotImplementedError()

    def get_color(self, socket):
        raise NotImplementedError()

class UniqueSocketBuilder(DataSocketBuilder):
    def __init__(self, socket_cls):
        self.socket_cls = socket_cls

    def build(self, node_sockets, name, identifier):
        return node_sockets.new(
            self.socket_cls.bl_idname,
            name,
            identifier=identifier)

    def get_color(self):
        return self.socket_cls.socket_color

class ColoredSocketBuilder(DataSocketBuilder):
    def __init__(self, color):
        self.color = color

    def build(self, node_sockets, name, identifier):
        socket = node_sockets.new(
            "fn_CustomColoredSocket",
            name,
            identifier=identifier)
        return socket

    def get_color(self):
        return self.color
