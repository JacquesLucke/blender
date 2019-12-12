#pragma once

#include "BLI_array_ref.h"

#include "BLI_rand.h"

namespace FN {

using BLI::ArrayRef;
using BLI::MutableArrayRef;

float compute_cumulative_distribution(ArrayRef<float> weights,
                                      MutableArrayRef<float> r_cumulative_weights);

void sample_cumulative_distribution(RNG *rng,
                                    ArrayRef<float> cumulative_weights,
                                    MutableArrayRef<uint> r_sampled_indices);

}  // namespace FN
