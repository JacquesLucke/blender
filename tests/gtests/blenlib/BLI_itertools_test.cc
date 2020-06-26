#include "BLI_itertools.hh"
#include "BLI_strict_flags.h"
#include "BLI_vector.hh"

#include "testing/testing.h"

namespace blender {

template<typename Container> class Wrapper {
  Container m_container;

 public:
  // Wrapper(Container container) : m_container(container)
  // {
  // }

  template<typename T> Wrapper(const T &container) : m_container(container)
  {
  }

  template<typename T> Wrapper(T &&container) : m_container(std::forward<T>(container))
  {
  }
};

template<typename T> Wrapper(const T &)->Wrapper<const T &>;
template<typename T> Wrapper(T &&)->Wrapper<T>;

TEST(itertools, EnumerateVector)
{
  auto variable = Vector<int>({4, 6, 7});
  // auto &&other = std::move(variable);

  Wrapper wrapper{variable};

  // Vector<Vector<int>> a;
  // a.append(std::forward<decltype(other)>(other));

  // std::cout << a.size() << "\n";
  // std::cout << other.size() << "\n";

  // for (auto &&[i, auto &&[first, second]] : enumerate(values)) {
  //   std::cout << i << " "
  //             << "\n";
  // }

  // auto &&a = 5;
}

}  // namespace blender
