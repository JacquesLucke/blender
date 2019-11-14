from collections import namedtuple, defaultdict

class DirectedGraph:
    def __init__(self, V, E):
        assert all(isinstance(e, tuple) for e in E)
        assert all(v1 in V and v2 in V for v1, v2 in E)
        self.V = set(V)
        self.E = set(E)

        self.outgoing = defaultdict(set)
        self.incoming = defaultdict(set)
        self.neighbors = defaultdict(set)
        for v1, v2 in E:
            self.outgoing[v1].add(v2)
            self.incoming[v2].add(v1)
            self.neighbors[v1].add(v2)
            self.neighbors[v2].add(v1)

    def inverted(self):
        return DirectedGraph(self.V, [(v2, v1) for v1, v2 in self.E])

    def reachable(self, start_verts):
        return self._reachable(start_verts, self.outgoing)

    def reachable_inversed(self, start_verts):
        return self._reachable(start_verts, self.incoming)

    def connected(self, start_verts):
        return self._reachable(start_verts, self.neighbors)

    def _reachable(self, start_verts, next_map):
        if start_verts in self.V:
            start_verts = (start_verts, )
        assert all(v in self.V for v in start_verts)

        verts_to_check = set(start_verts)
        found_verts = set()
        while len(verts_to_check) > 0:
            v = verts_to_check.pop()
            found_verts.add(v)
            for prev_v in next_map[v]:
                if prev_v not in found_verts:
                    verts_to_check.add(prev_v)
        return found_verts

    def toposort(self):
        return self.toposort_partial(self.V)

    def toposort_partial(self, verts_to_sort):
        verts_to_sort = set(verts_to_sort)
        sorted_verts = list()
        temp_marked_verts = set()
        finished_verts = set()

        def visit(v):
            if v in finished_verts:
                return
            if v in temp_marked_verts:
                raise Exception("not a DAG")
            temp_marked_verts.add(v)
            for prev_v in self.incoming[v]:
                visit(prev_v)
            finished_verts.add(v)
            if v in verts_to_sort:
                sorted_verts.append(v)

        for v in verts_to_sort:
            visit(v)

        return tuple(sorted_verts)


class DirectedGraphBuilder:
    def __init__(self):
        self.V = set()
        self.E = set()

    def add_vertex(self, v):
        self.V.add(v)

    def add_directed_edge(self, from_v, to_v):
        self.V.add(from_v)
        self.V.add(to_v)
        self.E.add((from_v, to_v))

    def build(self):
        return DirectedGraph(self.V, self.E)
