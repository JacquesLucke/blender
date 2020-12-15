/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_inplace_priority_queue.hh"

namespace blender::tests {

struct WeightsData {
  Array<float> weights;

  int64_t size() const
  {
    return weights.size();
  }

  std::string get_value_string(const int64_t index) const
  {
    return std::to_string(weights[index]);
  }

  bool is_higher_priority(const int64_t a, const int64_t b) const
  {
    return weights[a] > weights[b];
  }
};

TEST(inplace_priority_queue, BuildSmall)
{
  WeightsData data = {{1, 5, 2, 8, 5, 6, 5, 4, 3, 6, 7, 3}};
  InplacePriorityQueue priority_queue{data};
  priority_queue.build();

  Vector<float> sorted_data;

  while (!priority_queue.is_empty()) {
    sorted_data.append(data.weights[priority_queue.pop_top()]);
  }

  for (float v : sorted_data) {
    std::cout << v << ", ";
  }
  std::cout << "\n";

  std::cout << priority_queue.all_to_dot() << "\n";
}

}  // namespace blender::tests
