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

#pragma once

#include "FN_generic_pointer.hh"

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"

#include "BKE_geometry.hh"

namespace blender::nodes {

using bke::Geometry;
using fn::CPPType;
using fn::GMutablePointer;

class GeoNodeInputBuilder {
 private:
  Map<StringRef, GMutablePointer> values_;

  friend class GeoNodeInput;

 public:
  void add(StringRef name, const CPPType &type, void *data)
  {
    values_.add_new(name, GMutablePointer{type, data});
  }
};

class GeoNodeInput {
 private:
  const GeoNodeInputBuilder &builder_;

 public:
  GeoNodeInput(const GeoNodeInputBuilder &builder) : builder_(builder)
  {
  }

  GMutablePointer get(StringRef name)
  {
    return builder_.values_.lookup(name);
  }

  template<typename T> T &get(StringRef name)
  {
    return *builder_.values_.lookup(name).get<T>();
  }
};

class GeoNodeOutputCollector {
 private:
  LinearAllocator<> &allocator_;
  Map<StringRef, GMutablePointer> values_;

  friend class GeoNodeOutput;

 public:
  GeoNodeOutputCollector(LinearAllocator<> &allocator) : allocator_(allocator)
  {
  }
};

class GeoNodeOutput {
 private:
  GeoNodeOutputCollector &collector_;

 public:
  GeoNodeOutput(GeoNodeOutputCollector &collector) : collector_(collector)
  {
  }

  void set(StringRef name, const CPPType &type, void *data)
  {
    collector_.values_.add(name, GMutablePointer{type, data});
  }

  template<typename T> void set(StringRef name, T &&data)
  {
    T *copied_data = collector_.allocator_.construct<T>(std::forward<T>(data));
    collector_.values_.add_new(name, GMutablePointer{copied_data});
  }
};

struct GeometryP {
  bke::Geometry *p = nullptr;

  uint64_t hash() const
  {
    return DefaultHash<bke::Geometry *>{}(p);
  }

  friend std::ostream &operator<<(std::ostream &stream, const GeometryP &geometry)
  {
    stream << geometry.p;
    return stream;
  }

  friend bool operator==(const GeometryP &a, const GeometryP &b)
  {
    return a.p == b.p;
  }
};

}  // namespace blender::nodes
