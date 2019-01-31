set(INC
	.
)

set(INC_SYS
	${EIGEN3_INCLUDE_DIRS}
)

set(SRC
	source/CholUp/EliminationTreeMethods.cpp

	source/CholUp/EliminationTreeMethods.h
	source/CholUp/Matrix.hpp
	source/CholUp/SparseMatrix_impl.h
	source/CholUp/SparseMatrix.h
	source/CholUp/SparseSupernodal.h
	source/CholUp/SupernodalCholesky.h
	source/CholUp/CholUp.hpp
)

blender_add_lib(extern_cholup "${SRC}" "${INC}" "${INC_SYS}")
