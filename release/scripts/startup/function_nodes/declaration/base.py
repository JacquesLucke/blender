class SocketDeclBase:
    def build(self, node_sockets):
        raise NotImplementedError()

    def amount(self):
        raise NotImplementedError()

    def validate(self, sockets):
        raise NotImplementedError()

    def draw_node(self, layout):
        pass

    def draw_socket(self, layout, socket, index):
        socket.draw_self(layout, self)

    def operator_socket_call(self, own_socket, other_socket):
        pass