#include "DNA_modifier_types.h"

extern "C" {
void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts);
}

void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts)
{
  for (uint i = 0; i < numVerts; i++) {
    vertexCos[i][2] += 3;
  }
}