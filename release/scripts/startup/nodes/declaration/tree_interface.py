import typing as T

from . base import SocketDeclBase
from .. types import type_infos
from .. function_tree import FunctionTree

class TreeInterfaceDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, tree: FunctionTree, in_or_out: str, ignored: T.Set[T.Tuple[str, str]]):
        assert tree is not None
        self.node = node
        self.identifier = identifier
        self.tree = tree
        self.in_or_out = in_or_out
        self.ignored = ignored

    def build(self, node_sockets):
        if self.in_or_out == "IN":
            return list(self._build_inputs(node_sockets))
        elif self.in_or_out == "OUT":
            return list(self._build_outputs(node_sockets))
        else:
            assert False

    def _build_inputs(self, node_sockets):
        for data_type, name, identifier in self.iter_filtered_inputs():
            yield type_infos.build(
                data_type,
                node_sockets,
                name,
                self.identifier + identifier)

    def _build_outputs(self, node_sockets):
        for data_type, name, identifier in self.iter_filtered_outputs():
            yield type_infos.build(
                data_type,
                node_sockets,
                name,
                self.identifier + identifier)

    def validate(self, sockets):
        if self.in_or_out == "IN":
            return self.validate_in(sockets)
        elif self.in_or_out == "OUT":
            return self.validate_out(sockets)
        else:
            assert False

    def validate_in(self, sockets):
        params = list(self.iter_filtered_inputs())
        if len(params) != len(sockets):
            return False

        for param, socket in zip(params, sockets):
            identifier = self.identifier + param.identifier
            if not self._data_socket_test(socket, param.name, param.data_type, identifier):
                return False

        return True

    def validate_out(self, sockets):
        params = list(self.iter_filtered_outputs())
        if len(params) != len(sockets):
            return False

        for param, socket in zip(params, sockets):
            identifier = self.identifier + param.identifier
            if not self._data_socket_test(socket, param.name, param.data_type, identifier):
                return False

        return True

    def amount(self):
        if self.in_or_out == "IN":
            return len(tuple(self.iter_filtered_inputs()))
        elif self.in_or_out == "OUT":
            return len(tuple(self.iter_filtered_outputs()))
        else:
            assert False

    def iter_filtered_inputs(self):
        for item in self.tree.iter_function_inputs():
            if (item.data_type, item.name) not in self.ignored:
                yield item

    def iter_filtered_outputs(self):
        for item in self.tree.iter_function_outputs():
            if (item.data_type, item.name) not in self.ignored:
                yield item
