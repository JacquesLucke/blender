import bpy
from collections import namedtuple

from . base import BaseTree, BaseNode
from . graph import DirectedGraphBuilder, DirectedGraph

class FunctionTree(bpy.types.NodeTree, BaseTree):
    bl_idname = "FunctionTree"
    bl_icon = "MOD_DATA_TRANSFER"
    bl_label = "Function Nodes"

    def get_input_nodes(self):
        input_nodes = [node for node in self.nodes if node.bl_idname == "fn_GroupInputNode"]
        sorted_input_nodes = sorted(input_nodes, key=lambda node: (node.sort_index, node.name))
        return sorted_input_nodes

    def get_output_nodes(self):
        output_nodes = [node for node in self.nodes if node.bl_idname == "fn_GroupOutputNode"]
        sorted_output_nodes = sorted(output_nodes, key=lambda node: (node.sort_index, node.name))
        return sorted_output_nodes

    def get_directly_used_trees(self):
        trees = set()
        for node in self.nodes:
            if isinstance(node, BaseNode):
                trees.update(node.iter_directly_used_trees())
        return trees

    def find_callable_trees(self):
        used_by_trees = FunctionTree.BuildInvertedCallGraph().reachable(self)
        trees = [tree for tree in bpy.data.node_groups
                 if isinstance(tree, FunctionTree) and tree not in used_by_trees]
        return trees

    @staticmethod
    def BuildTreeCallGraph() -> DirectedGraph:
        '''
        Every vertex is a tree.
        Every edge (A, B) means: Tree A uses tree B.
        '''
        builder = DirectedGraphBuilder()
        for tree in bpy.data.node_groups:
            if isinstance(tree, FunctionTree):
                builder.add_vertex(tree)
                for dependency_tree in tree.get_directly_used_trees():
                    builder.add_directed_edge(
                        from_v=tree,
                        to_v=dependency_tree)
        return builder.build()

    @staticmethod
    def BuildInvertedCallGraph() -> DirectedGraph:
        '''
        Builds a directed graph in which every tree is a vertex.
        Every edge (A, B) means: Changes in A might affect B.
        '''
        return FunctionTree.BuildTreeCallGraph().inverted()
