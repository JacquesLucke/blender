/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FN_MULTI_FUNCTION_PARAM_TYPE_HH__
#define __FN_MULTI_FUNCTION_PARAM_TYPE_HH__

#include "FN_multi_function_data_type.hh"

namespace FN {

class MFParamType {
 public:
  enum InterfaceType {
    Input,
    Output,
    Mutable,
  };

  enum Category {
    SingleInput,
    VectorInput,
    SingleOutput,
    VectorOutput,
    SingleMutable,
    VectorMutable,
  };

 private:
  InterfaceType m_interface_type;
  MFDataType m_data_type;

 public:
  MFParamType(InterfaceType interface_type, MFDataType data_type)
      : m_interface_type(interface_type), m_data_type(data_type)
  {
  }

  MFDataType data_type() const
  {
    return m_data_type;
  }

  InterfaceType interface_type() const
  {
    return m_interface_type;
  }

  Category category() const
  {
    switch (m_data_type.category()) {
      case MFDataType::Single: {
        switch (m_interface_type) {
          case Input:
            return SingleInput;
          case Output:
            return SingleOutput;
          case Mutable:
            return SingleMutable;
        }
        break;
      }
      case MFDataType::Vector: {
        switch (m_interface_type) {
          case Input:
            return VectorInput;
          case Output:
            return VectorOutput;
          case Mutable:
            return VectorMutable;
        }
        break;
      }
    }
    BLI_assert(false);
    return SingleInput;
  }

  friend bool operator==(const MFParamType &a, const MFParamType &b)
  {
    return a.m_interface_type == b.m_interface_type && a.m_data_type == b.m_data_type;
  }

  friend bool operator!=(const MFParamType &a, const MFParamType &b)
  {
    return !(a == b);
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_PARAM_TYPE_HH__ */
