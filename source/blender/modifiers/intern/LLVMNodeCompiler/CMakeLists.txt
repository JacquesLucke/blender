set(INC
	.
	../../../blenlib
)

set(INC_SYS
	/usr/lib/llvm-6.0/include
)

set(SRC
    anysocket.cpp
    dataflowcallable.cpp
    dataflowgraph.cpp
    dot_export.cpp
    ir_utils.cpp
    linkset.cpp
    node_utils.cpp
    node.cpp
    nodesockets.cpp
    type.cpp
    link.cpp
    socketinfo.cpp

    ArraySet.hpp
    core_types.hpp
    core.hpp
    HashMap.hpp
    node_utils.hpp
    type_utils.hpp
    ir_utils.hpp

)

blender_add_lib(bf_llvm_node_compiler "${SRC}" "${INC}" "${INC_SYS}")
