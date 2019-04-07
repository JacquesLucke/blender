from unittest import TestCase
from . graph import DirectedGraph, DirectedGraphBuilder

class TestGraphToposort(TestCase):
    def test_empty_graph(self):
        graph = DirectedGraph(set(), set())
        self.assertEqual(graph.toposort(), tuple())

    def test_single_vertex_graph(self):
        graph = DirectedGraph({1}, set())
        self.assertEqual(graph.toposort(), (1, ))

    def test_connected_graph(self):
        builder = DirectedGraphBuilder()
        builder.add_directed_edge(2, 1)
        builder.add_directed_edge(4, 1)
        builder.add_directed_edge(4, 3)
        builder.add_directed_edge(3, 1)
        builder.add_directed_edge(3, 2)
        graph = builder.build()
        self.assertEqual(graph.toposort(), (4, 3, 2, 1))

    def test_partial(self):
        builder = DirectedGraphBuilder()
        builder.add_directed_edge(4, 3)
        builder.add_directed_edge(1, 4)
        builder.add_directed_edge(5, 2)
        builder.add_directed_edge(3, 5)
        graph = builder.build()
        self.assertEqual(graph.toposort_partial({5, 4, 2}), (4, 5, 2))

    def test_no_dag(self):
        builder = DirectedGraphBuilder()
        builder.add_directed_edge(1, 2)
        builder.add_directed_edge(2, 1)
        graph = builder.build()
        with self.assertRaises(Exception):
            graph.toposort()
