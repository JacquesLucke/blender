# Type Info Container
#####################################

class DataTypesInfo:
    def __init__(self):
        self.data_types = set()
        self.builder_by_data_type = dict()
        self.list_by_base = dict()
        self.base_by_list = dict()

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
        for data_type in self.list_by_base.keys():
            items.append((data_type, data_type, ""))
        return items

    def get_data_type_items_cb(self):
        def callback(_1, _2):
            return self.get_data_type_items()
        return callback

    def get_socket_color(self, data_type):
        builder = self.to_builder(data_type)
        return builder.get_color()


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
        socket.color = self.color
        return socket

    def get_color(self):
        return self.color