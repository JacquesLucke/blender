class SocketDeclBase:
    def init(self):
        pass

    def build(self, node_sockets):
        raise NotImplementedError()

    def init_default(self, node_sockets):
        pass

    def amount(self):
        raise NotImplementedError()

    def validate(self, sockets):
        raise NotImplementedError()

    def draw_node(self, layout):
        pass

    def draw_socket(self, layout, socket, index):
        socket.draw_self(layout, self, socket.name)

    def operator_socket_call(self, own_socket, other_socket):
        pass

    def _data_socket_test(self, socket, name, data_type, identifier):
        from .. base import DataSocket
        if not isinstance(socket, DataSocket):
            return False
        if socket.name != name:
            return False
        if socket.data_type != data_type:
            return False
        if socket.identifier != identifier:
            return False
        return True
