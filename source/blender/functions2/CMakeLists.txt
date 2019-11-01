set(INC
  .
  ../blenlib
  ../makesdna
  ../makesrna
  ../blenkernel
  ../depsgraph
  ../windowmanager
  ../../../intern/guardedalloc
)

set(INC_SYS
  ${LLVM_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

set(SRC
  intern/multi_functions/lists.cc
  intern/multi_functions/mixed.cc
  intern/cpp_type.cc
  intern/cpp_types.cc
  intern/generic_tuple.cc
  intern/initialize.cc
  intern/multi_function_network.cc

  FN_cpp_type.h
  FN_generic_array_ref.h
  FN_generic_tuple.h
  FN_generic_vector_array.h
  FN_generic_virtual_list_list_ref.h
  FN_generic_virtual_list_ref.h
  FN_initialize.h
  FN_multi_function_network.h
  FN_multi_function.h
  FN_multi_functions.h
  FN_vtree_multi_function_network.h

  intern/multi_functions/lists.h
  intern/multi_functions/mixed.h
  intern/cpp_types.h
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_functions2 "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
