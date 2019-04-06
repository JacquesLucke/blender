import itertools
from collections import namedtuple


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

    def insert_data_type(self, data_type, builder):
        assert data_type not in self.data_types
        assert isinstance(builder, DataSocketBuilder)

        self.data_types.add(data_type)
        self.builder_by_data_type[data_type] = builder

    def insert_list_relation(self, base_type, list_type):
        assert self.is_data_type(base_type)
        assert self.is_data_type(list_type)
        assert base_type not in self.list_by_base
        assert list_type not in self.base_by_list

        self.list_by_base[base_type] = list_type
        self.base_by_list[list_type] = base_type

        self.insert_implicit_conversion(base_type, list_type)

    def insert_implicitly_convertable_types(self, types):
        for type_1, type_2 in itertools.combinations(types, 2):
            self.insert_implicit_conversion(type_1, type_2)
            self.insert_implicit_conversion(type_2, type_1)

    def insert_implicit_conversion(self, from_type, to_type):
        assert self.is_data_type(from_type)
        assert self.is_data_type(to_type)

        conversion = ImplicitConversion(from_type, to_type)
        assert conversion not in self.implicit_conversions
        self.implicit_conversions.add(conversion)


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