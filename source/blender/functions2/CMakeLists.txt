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
  intern/cpp_type.cc
  intern/cpp_types.cc
  intern/initialize.cc

  FN_cpp_type.h
  FN_initialize.h

  intern/cpp_types.h
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_functions2 "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
