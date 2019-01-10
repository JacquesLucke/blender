#pragma once

#include "nodecompiler/core.hpp"

namespace NC = LLVMNodeCompiler;

struct Vector3 {
	float x, y, z;
};

extern NC::Type *type_int32;
extern NC::Type *type_float;
extern NC::Type *type_vec3;