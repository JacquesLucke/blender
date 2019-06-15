/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Turbulence */

ccl_device_noinline float noise_turbulence(float3 p, float octaves, int hard)
{
  octaves = clamp(octaves, 0.0f, 16.0f);
  int n = float_to_int(octaves);

  int i = n;
  float sum = 0.0f;
  float fscale = 1.0f;
  float amp = 1.0f;
  while (i > 0) {
#ifdef __KERNEL_SSE2__
    if (i > 4) {
      ssef x = ssef(p.x, p.x * 2.0f, p.x * 4.0f, p.x * 8.0f);
      ssef y = ssef(p.y, p.y * 2.0f, p.y * 4.0f, p.y * 8.0f);
      ssef z = ssef(p.z, p.z * 2.0f, p.z * 4.0f, p.z * 8.0f);
      ssef t = noise(x, y, z);
      t *= ssef(amp, amp * 0.5f, amp * 0.25f, amp * 0.125f);

      float ts[4];
      store4f(ts, t);
      sum += ts[0] + ts[1] + ts[2] + ts[3];

      fscale *= 16.0f;
      amp *= 0.06125f;
      i -= 4;
      continue;
    }
#endif
    float t = noise(fscale * p);
    sum += t * amp;
    amp *= 0.5f;
    fscale *= 2.0f;
    i -= 1;
  }

  float rmd = octaves - floorf(octaves);
  if (rmd != 0.0f) {
    float t = noise(fscale * p);

    if (hard)
      t = fabsf(2.0f * t - 1.0f);

    float sum2 = sum + t * amp;

    sum *= ((float)(1 << n) / (float)((1 << (n + 1)) - 1));
    sum2 *= ((float)(1 << (n + 1)) / (float)((1 << (n + 2)) - 1));

    return (1.0f - rmd) * sum + rmd * sum2;
  }
  else {
    sum *= ((float)(1 << n) / (float)((1 << (n + 1)) - 1));
    return sum;
  }
}

CCL_NAMESPACE_END
