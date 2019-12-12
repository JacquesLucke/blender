#include "sampling_util.h"

#include "BLI_vector_adaptor.h"

namespace FN {

using BLI::VectorAdaptor;

float compute_cumulative_distribution(ArrayRef<float> weights,
                                      MutableArrayRef<float> r_cumulative_weights)
{
  BLI_assert(weights.size() + 1 == r_cumulative_weights.size());

  r_cumulative_weights[0] = 0;
  for (uint i : weights.index_iterator()) {
    r_cumulative_weights[i + 1] = r_cumulative_weights[i] + weights[i];
  }

  float weight_sum = r_cumulative_weights.last();
  return weight_sum;
}

static void sample_cumulative_distribution__recursive(RNG *rng,
                                                      uint amount,
                                                      uint start,
                                                      uint one_after_end,
                                                      ArrayRef<float> cumulative_weights,
                                                      VectorAdaptor<uint> &sampled_indices)
{
  BLI_assert(start <= one_after_end);
  uint size = one_after_end - start;
  if (size == 0) {
    BLI_assert(amount == 0);
  }
  else if (amount == 0) {
    return;
  }
  else if (size == 1) {
    sampled_indices.append_n_times(start, amount);
  }
  else {
    uint middle = start + size / 2;
    float left_weight = cumulative_weights[middle] - cumulative_weights[start];
    float right_weight = cumulative_weights[one_after_end] - cumulative_weights[middle];
    BLI_assert(left_weight >= 0.0f && right_weight >= 0.0f);
    float weight_sum = left_weight + right_weight;
    BLI_assert(weight_sum > 0.0f);

    float left_factor = left_weight / weight_sum;
    float right_factor = right_weight / weight_sum;

    uint left_amount = amount * left_factor;
    uint right_amount = amount * right_factor;

    if (left_amount + right_amount < amount) {
      BLI_assert(left_amount + right_amount + 1 == amount);
      float weight_per_item = weight_sum / amount;
      float total_remaining_weight = weight_sum - (left_amount + right_amount) * weight_per_item;
      float left_remaining_weight = left_weight - left_amount * weight_per_item;
      float left_remaining_factor = left_remaining_weight / total_remaining_weight;
      if (BLI_rng_get_float(rng) < left_remaining_factor) {
        left_amount++;
      }
      else {
        right_amount++;
      }
    }

    sample_cumulative_distribution__recursive(
        rng, left_amount, start, middle, cumulative_weights, sampled_indices);
    sample_cumulative_distribution__recursive(
        rng, right_amount, middle, one_after_end, cumulative_weights, sampled_indices);
  }
}

void sample_cumulative_distribution(RNG *rng,
                                    ArrayRef<float> cumulative_weights,
                                    MutableArrayRef<uint> r_sampled_indices)
{
  BLI_assert(r_sampled_indices.size() == 0 || cumulative_weights.last() > 0.0f);

  uint amount = r_sampled_indices.size();
  VectorAdaptor<uint> sampled_indices(r_sampled_indices.begin(), amount);
  sample_cumulative_distribution__recursive(
      rng, amount, 0, cumulative_weights.size() - 1, cumulative_weights, sampled_indices);
  BLI_assert(sampled_indices.is_full());
}

}  // namespace FN
