#include "FastSVD.hpp"

#include <cmath>

#undef USE_SCALAR_IMPLEMENTATION
#define USE_SSE_IMPLEMENTATION
#undef USE_AVX_IMPLEMENTATION
#define COMPUTE_U_AS_MATRIX
#undef COMPUTE_U_AS_QUATERNION
#define COMPUTE_V_AS_MATRIX
#undef COMPUTE_V_AS_QUATERNION
#include "Singular_Value_Decomposition_Preamble.hpp"

void fastSVD_SSE(float a[9][4])
{
#include "Singular_Value_Decomposition_Kernel_Declarations.hpp"

    ENABLE_SSE_IMPLEMENTATION(Va11=_mm_loadu_ps(a[0]);)
    ENABLE_SSE_IMPLEMENTATION(Va21=_mm_loadu_ps(a[1]);)
    ENABLE_SSE_IMPLEMENTATION(Va31=_mm_loadu_ps(a[2]);)
    ENABLE_SSE_IMPLEMENTATION(Va12=_mm_loadu_ps(a[3]);)
    ENABLE_SSE_IMPLEMENTATION(Va22=_mm_loadu_ps(a[4]);)
    ENABLE_SSE_IMPLEMENTATION(Va32=_mm_loadu_ps(a[5]);)
    ENABLE_SSE_IMPLEMENTATION(Va13=_mm_loadu_ps(a[6]);)
    ENABLE_SSE_IMPLEMENTATION(Va23=_mm_loadu_ps(a[7]);)
    ENABLE_SSE_IMPLEMENTATION(Va33=_mm_loadu_ps(a[8]);)

#include "Singular_Value_Decomposition_Main_Kernel_Body.hpp"


    ENABLE_SSE_IMPLEMENTATION(__m128 R11;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R21;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R31;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R12;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R22;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R32;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R13;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R23;)
    ENABLE_SSE_IMPLEMENTATION(__m128 R33;)

    ENABLE_SSE_IMPLEMENTATION(__m128 tmp;)


    ENABLE_SSE_IMPLEMENTATION(R11 = _mm_mul_ps(Vv11, Vu11));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv12, Vu12));
    ENABLE_SSE_IMPLEMENTATION(R11 = _mm_add_ps(R11, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv13, Vu13));
    ENABLE_SSE_IMPLEMENTATION(R11 = _mm_add_ps(R11, tmp));

    ENABLE_SSE_IMPLEMENTATION(R21 = _mm_mul_ps(Vv21, Vu11));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv22, Vu12));
    ENABLE_SSE_IMPLEMENTATION(R21 = _mm_add_ps(R21, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv23, Vu13));
    ENABLE_SSE_IMPLEMENTATION(R21 = _mm_add_ps(R21, tmp));

    ENABLE_SSE_IMPLEMENTATION(R31 = _mm_mul_ps(Vv31, Vu11));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv32, Vu12));
    ENABLE_SSE_IMPLEMENTATION(R31 = _mm_add_ps(R31, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv33, Vu13));
    ENABLE_SSE_IMPLEMENTATION(R31 = _mm_add_ps(R31, tmp));


    ENABLE_SSE_IMPLEMENTATION(R12 = _mm_mul_ps(Vv11, Vu21));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv12, Vu22));
    ENABLE_SSE_IMPLEMENTATION(R12 = _mm_add_ps(R12, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv13, Vu23));
    ENABLE_SSE_IMPLEMENTATION(R12 = _mm_add_ps(R12, tmp));

    ENABLE_SSE_IMPLEMENTATION(R22 = _mm_mul_ps(Vv21, Vu21));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv22, Vu22));
    ENABLE_SSE_IMPLEMENTATION(R22 = _mm_add_ps(R22, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv23, Vu23));
    ENABLE_SSE_IMPLEMENTATION(R22 = _mm_add_ps(R22, tmp));

    ENABLE_SSE_IMPLEMENTATION(R32 = _mm_mul_ps(Vv31, Vu21));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv32, Vu22));
    ENABLE_SSE_IMPLEMENTATION(R32 = _mm_add_ps(R32, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv33, Vu23));
    ENABLE_SSE_IMPLEMENTATION(R32 = _mm_add_ps(R32, tmp));


    ENABLE_SSE_IMPLEMENTATION(R13 = _mm_mul_ps(Vv11, Vu31));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv12, Vu32));
    ENABLE_SSE_IMPLEMENTATION(R13 = _mm_add_ps(R13, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv13, Vu33));
    ENABLE_SSE_IMPLEMENTATION(R13 = _mm_add_ps(R13, tmp));

    ENABLE_SSE_IMPLEMENTATION(R23 = _mm_mul_ps(Vv21, Vu31));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv22, Vu32));
    ENABLE_SSE_IMPLEMENTATION(R23 = _mm_add_ps(R23, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv23, Vu33));
    ENABLE_SSE_IMPLEMENTATION(R23 = _mm_add_ps(R23, tmp));

    ENABLE_SSE_IMPLEMENTATION(R33 = _mm_mul_ps(Vv31, Vu31));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv32, Vu32));
    ENABLE_SSE_IMPLEMENTATION(R33 = _mm_add_ps(R33, tmp));
    ENABLE_SSE_IMPLEMENTATION(tmp = _mm_mul_ps(Vv33, Vu33));
    ENABLE_SSE_IMPLEMENTATION(R33 = _mm_add_ps(R33, tmp));


    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[0],R11);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[1],R21);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[2],R31);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[3],R12);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[4],R22);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[5],R32);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[6],R13);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[7],R23);)
    ENABLE_SSE_IMPLEMENTATION(_mm_storeu_ps(a[8],R33);)
}
