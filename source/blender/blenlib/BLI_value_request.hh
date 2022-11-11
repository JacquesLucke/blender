/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

class ValueRequest {
 public:
  virtual ~ValueRequest() = default;

  virtual void merge(const ValueRequest *other) = 0;
};

}  // namespace blender
