from . declaration import (
    FixedSocketDecl,
    ListSocketDecl,
    PackListDecl,
    AnyVariadicDecl,
    TreeInterfaceDecl,
)

class SocketBuilder:
    def __init__(self, node):
        self.node = node
        self.input_declarations = []
        self.output_declarations = []

    def _add_input(self, decl):
        self.input_declarations.append(decl)

    def _add_output(self, decl):
        self.output_declarations.append(decl)

    def build(self):
        self.node.inputs.clear()
        self.node.outputs.clear()

        for decl in self.input_declarations:
            sockets = decl.build(self.node.inputs)
            assert len(sockets) == decl.amount()

        for decl in self.output_declarations:
            sockets = decl.build(self.node.outputs)
            assert len(sockets) == decl.amount()

    def get_sockets_decl_map(self):
        return SocketDeclMap(
            self.node,
            self.input_declarations,
            self.output_declarations)


    # Fixed
    ###################################

    def fixed_input(self, identifier, name, data_type):
        decl = FixedSocketDecl(self.node, identifier, name, data_type)
        self._add_input(decl)

    def fixed_output(self, identifier, name, data_type):
        decl = FixedSocketDecl(self.node, identifier, name, data_type)
        self._add_output(decl)


    # Packed List
    ###################################

    @staticmethod
    def PackListProperty():
        return PackListDecl.Property()

    def pack_list_input(self, identifier, prop_name, base_type):
        decl = PackListDecl(self.node, identifier, prop_name, base_type)
        self._add_input(decl)


    # Dynamic List
    ###################################

    @staticmethod
    def ListTypeProperty():
        return ListSocketDecl.Property()

    def dynamic_list_input(self, identifier, name, prop_name):
        decl = ListSocketDecl(self.node, identifier, name, prop_name, "LIST")
        self._add_input(decl)

    def dynamic_list_output(self, identifier, name, prop_name):
        decl = ListSocketDecl(self.node, identifier, name, prop_name, "LIST")
        self._add_output(decl)

    def dynamic_base_input(self, identifier, name, prop_name):
        decl = ListSocketDecl(self.node, identifier, name, prop_name, "BASE")
        self._add_input(decl)

    def dynamic_base_output(self, identifier, name, prop_name):
        decl = ListSocketDecl(self.node, identifier, name, prop_name, "BASE")
        self._add_output(decl)


    # Variadic
    ##################################

    @staticmethod
    def VariadicProperty():
        return AnyVariadicDecl.Property()

    def variadic_input(self, identifier, prop_name, message):
        decl = AnyVariadicDecl(self.node, identifier, prop_name, message)
        self._add_input(decl)

    def variadic_output(self, identifier, prop_name, message):
        decl = AnyVariadicDecl(self.node, identifier, prop_name, message)
        self._add_output(decl)


    # Tree Interface
    ##################################

    def tree_interface_input(self, identifier, tree, in_or_out):
        decl = TreeInterfaceDecl(self.node, identifier, tree, in_or_out)
        self._add_input(decl)

    def tree_interface_output(self, identifier, tree, in_or_out):
        decl = TreeInterfaceDecl(self.node, identifier, tree, in_or_out)
        self._add_output(decl)


class SocketDeclMap:
    def __init__(self, node, input_declarations, output_declarations):
        self._sockets_by_decl = dict()
        self._decl_by_socket = dict()
        self._socket_index_in_decl = dict()

        for decl, sockets in iter_sockets_by_decl(node.inputs, input_declarations):
            self._sockets_by_decl[decl] = sockets
            for i, socket in enumerate(sockets):
                self._decl_by_socket[socket] = decl
                self._socket_index_in_decl[socket] = i

        for decl, sockets in iter_sockets_by_decl(node.outputs, output_declarations):
            self._sockets_by_decl[decl] = sockets
            for i, socket in enumerate(sockets):
                self._decl_by_socket[socket] = decl
                self._socket_index_in_decl[socket] = i

    def get_decl_by_socket(self, socket):
        return self._decl_by_socket[socket]

    def get_socket_index_in_decl(self, socket):
        return self._socket_index_in_decl[socket]

    def get_sockets_by_decl(self, decl):
        return self._sockets_by_decl[decl]

    def iter_decl_with_sockets(self):
        yield from self._sockets_by_decl.items()

    def iter_decls(self):
        yield from self._sockets_by_decl.keys()


def iter_sockets_by_decl(node_sockets, declarations):
    node_sockets_iter = iter(node_sockets)
    for decl in declarations:
        amount = decl.amount()
        sockets_of_decl = tuple(next(node_sockets_iter) for _ in range(amount))
        assert decl.validate(sockets_of_decl)
        yield decl, sockets_of_decl