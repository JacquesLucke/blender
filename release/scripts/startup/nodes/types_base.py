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

A type can be in zero or one conversion group.
Every type in this group can be converted to any other implicitely.
The types within a group are ordered by their "rank".
When two types with different rank are used in one expression,
the type with lower rank is converted to the other.
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
        self.unidirectional_conversions = set()
        self.conversion_groups = dict()
        self.all_implicit_conversions = set()


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

        self.all_implicit_conversions.add(ImplicitConversion(base_type, list_type))

    def insert_conversion_group(self, types_by_rank):
        '''lowest rank comes first'''

        for data_type in types_by_rank:
            assert self.is_data_type(data_type)
            assert self.is_base(data_type)
            assert data_type not in self.conversion_groups

        group = tuple(types_by_rank)
        for data_type in types_by_rank:
            self.conversion_groups[data_type] = group

        for from_base_type, to_base_type in itertools.combinations(group, 2):
            from_list_type = self.to_list(from_base_type)
            to_list_type = self.to_list(to_base_type)
            self.all_implicit_conversions.add(ImplicitConversion(from_base_type, to_base_type))
            self.all_implicit_conversions.add(ImplicitConversion(to_base_type, from_base_type))
            self.all_implicit_conversions.add(ImplicitConversion(from_list_type, to_list_type))
            self.all_implicit_conversions.add(ImplicitConversion(to_list_type, from_list_type))

    def insert_unidirectional_conversion(self, from_type, to_type):
        assert self.is_data_type(from_type)
        assert self.is_data_type(to_type)
        assert self.is_base(from_type)
        assert self.is_base(to_type)

        base_conversion = ImplicitConversion(from_type, to_type)
        assert base_conversion not in self.implicit_conversions
        self.implicit_conversions.add(base_conversion)
        self.all_implicit_conversions.add(base_conversion)

        list_conversion = ImplicitConversion(
            self.to_list(from_type), self.to_list(to_type))
        assert list_conversion not in self.implicit_conversions
        self.implicit_conversions.add(list_conversion)
        self.all_implicit_conversions.add(list_conversion)


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
        return ImplicitConversion(from_type, to_type) in self.all_implicit_conversions

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
