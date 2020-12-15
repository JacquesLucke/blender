/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_inplace_priority_queue.hh"

namespace blender::tests {

TEST(inplace_priority_queue, BuildSmall)
{
  Array<float> data = {1, 5, 2, 8, 5, 6, 5, 4, 3, 6, 7, 3};
  InplacePriorityQueue<float> priority_queue{data};
  priority_queue.build();

  data[1] = 30;
  priority_queue.priority_changed(1);
  data[1] = 7.5f;
  priority_queue.priority_changed(1);

  // Vector<float> sorted_data;

  // while (!priority_queue.is_empty()) {
  //   sorted_data.append(data.weights[priority_queue.pop_top()]);
  // }

  // for (float v : sorted_data) {
  //   std::cout << v << ", ";
  // }
  // std::cout << "\n";

  std::cout << priority_queue.all_to_dot() << "\n";
}

}  // namespace blender::tests
