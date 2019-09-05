#pragma once

#include "FN_core.hpp"

namespace FN {
namespace Types {

/* Still have to figure out a better way to handle strings. Calling it MyString for now until a
 * better name is found. std::string cannot easily be used because it would need special handling
 * as llvm type as well. */
class MyString {
 private:
  char *m_string;

 public:
  MyString() : m_string(nullptr)
  {
  }

  MyString(StringRef str_ref)
  {
    m_string = (char *)MEM_mallocN(str_ref.size() + 1, __func__);
    str_ref.copy_to__with_null(m_string);
  }

  ~MyString()
  {
    if (m_string != nullptr) {
      MEM_freeN(m_string);
    }
  }

  MyString(const MyString &other) : MyString(StringRef(other))
  {
  }

  MyString(MyString &&other)
  {
    m_string = other.m_string;
    other.m_string = nullptr;
  }

  MyString &operator=(const MyString &other)
  {
    if (this == &other) {
      return *this;
    }

    this->~MyString();
    new (this) MyString(other);
    return *this;
  }

  MyString &operator=(MyString &&other)
  {
    if (this == &other) {
      return *this;
    }

    this->~MyString();
    new (this) MyString(std::move(other));
    return *this;
  }

  const char *data() const
  {
    return m_string;
  }

  operator StringRefNull() const
  {
    if (m_string == nullptr) {
      return StringRefNull();
    }
    else {
      return StringRefNull(m_string);
    }
  }

  uint size() const
  {
    if (m_string == nullptr) {
      return 0;
    }
    else {
      return strlen(m_string);
    }
  }
};

void INIT_string(Vector<Type *> &types_to_free);

extern Type *TYPE_string;
extern Type *TYPE_string_list;

}  // namespace Types
}  // namespace FN
