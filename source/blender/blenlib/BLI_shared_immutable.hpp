#pragma once

/* A shared immutable type has a reference count and
 * is freed automatically, when it is not used anymore.
 * Furthermore, it must not be modified, when it is
 * referenced in two or more places.
 *
 * When the reference is one, it can be mutated.
 *
 * This approach reduces the amount of defensive
 * copies of data (data that is copied to make sure
 * that nobody does not change it anymore). Instead,
 * to one just have to increase the user count.
 *
 * A copy has to be made, when the user count is >= 2.
 *
 * Reference counting can be automated with the
 * AutoRefCount class.
 */

#include "BLI_shared.hpp"

namespace BLI {

class SharedImmutable : protected RefCountedBase {
 private:
  SharedImmutable(SharedImmutable &other) = delete;

  template<typename> friend class AutoRefCount;

 public:
  SharedImmutable() : RefCountedBase()
  {
  }

  virtual ~SharedImmutable()
  {
  }

  void new_user()
  {
    this->incref();
  }

  void remove_user()
  {
    this->decref();
  }

  int users() const
  {
    return this->refcount();
  }

  bool is_mutable() const
  {
    return this->users() == 1;
  }

  bool is_immutable() const
  {
    return this->users() > 1;
  }

  void assert_mutable() const
  {
    BLI_assert(this->is_mutable());
  }
};

} /* namespace BLI */
