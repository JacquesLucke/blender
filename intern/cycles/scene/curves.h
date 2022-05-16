/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __CURVES_H__
#define __CURVES_H__

#include "util/array.h"
#include "util/types.h"

#include "scene/hair.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

void curvebounds(float *lower, float *upper, float3 *p, int dim);

CCL_NAMESPACE_END

#endif /* __CURVES_H__ */
