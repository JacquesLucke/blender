set(INC
	.
)

set(INC_SYS

)

set(SRC
    ./source/FastSVD.cpp

    ./source/FastSVD.hpp
    ./source/Singular_Value_Decomposition_Givens_QR_Factorization_Kernel.hpp
    ./source/Singular_Value_Decomposition_Jacobi_Conjugation_Kernel.hpp
    ./source/Singular_Value_Decomposition_Kernel_Declarations.hpp
    ./source/Singular_Value_Decomposition_Main_Kernel_Body.hpp
    ./source/Singular_Value_Decomposition_Preamble.hpp
)

blender_add_lib(bf_intern_fast_svd "${SRC}" "${INC}" "${INC_SYS}")
