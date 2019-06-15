/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_SSE2__
ccl_device_inline ssei quick_floor_sse(const ssef &x)
{
  ssei b = truncatei(x);
  ssei isneg = cast((x < ssef(0.0f)).m128);
  return b + isneg;  // unsaturated add 0xffffffff is the same as subtract -1
}
#endif

static float interpolate_trilinear(
    float t1, float t2, float t3, ssef vs_000_001_010_011, ssef vs_100_101_110_111)
{
  float t1_inv = 1.0f - t1;
  float t2_inv = 1.0f - t2;
  float t3_inv = 1.0f - t3;

  ssef vs_t00_t01_t10_t11 = vs_000_001_010_011 * t1_inv + vs_100_101_110_111 * t1;
  ssef vs_t01_x_t11_x = shuffle<1, 0, 3, 0>(vs_t00_t01_t10_t11);
  ssef vs_t0t_x_t1t_x = vs_t00_t01_t10_t11 * t3_inv + vs_t01_x_t11_x * t3;
  ssef vs_t1t_x_x_x = shuffle<2, 0, 0, 0>(vs_t0t_x_t1t_x);
  ssef vs_ttt_x_x_x = vs_t0t_x_t1t_x * t2_inv + vs_t1t_x_x_x * t2;

  return extract<0>(vs_ttt_x_x_x);
}

static float float_lookup_table[256] = {0.41960784313725497f,
                                        0.5450980392156863f,
                                        -0.4117647058823529f,
                                        -0.5372549019607843f,
                                        -0.403921568627451f,
                                        0.12941176470588234f,
                                        -0.04313725490196074f,
                                        0.0980392156862746f,
                                        -0.9529411764705882f,
                                        0.16862745098039222f,
                                        0.7725490196078431f,
                                        0.0117647058823529f,
                                        -0.584313725490196f,
                                        -0.7568627450980392f,
                                        0.9529411764705882f,
                                        0.8509803921568628f,
                                        0.37254901960784315f,
                                        0.07450980392156858f,
                                        1.0f,
                                        -0.23921568627450984f,
                                        -0.2784313725490196f,
                                        -0.7176470588235294f,
                                        -0.7803921568627451f,
                                        -0.09019607843137256f,
                                        0.8352941176470587f,
                                        -0.4509803921568627f,
                                        -0.3647058823529412f,
                                        0.7490196078431373f,
                                        -0.7725490196078432f,
                                        -0.41960784313725485f,
                                        -0.8980392156862745f,
                                        0.7411764705882353f,
                                        0.3411764705882352f,
                                        -0.19215686274509802f,
                                        -0.13725490196078427f,
                                        -0.9686274509803922f,
                                        -0.2705882352941177f,
                                        0.3254901960784313f,
                                        -0.4901960784313726f,
                                        0.027450980392156765f,
                                        -0.0980392156862745f,
                                        0.6627450980392158f,
                                        -0.9450980392156862f,
                                        -0.45882352941176474f,
                                        0.41176470588235303f,
                                        -0.15294117647058825f,
                                        -0.1215686274509804f,
                                        -0.050980392156862786f,
                                        0.9450980392156862f,
                                        0.08235294117647052f,
                                        0.10588235294117654f,
                                        0.584313725490196f,
                                        -0.26274509803921564f,
                                        -0.5764705882352941f,
                                        0.6862745098039216f,
                                        0.5294117647058822f,
                                        0.9372549019607843f,
                                        -0.5137254901960784f,
                                        -0.07450980392156858f,
                                        -0.6470588235294117f,
                                        0.19999999999999996f,
                                        0.6000000000000001f,
                                        0.04313725490196085f,
                                        0.5137254901960784f,
                                        0.44313725490196076f,
                                        0.9215686274509804f,
                                        0.4274509803921569f,
                                        -0.019607843137254943f,
                                        -0.7098039215686274f,
                                        0.050980392156862786f,
                                        -0.3411764705882353f,
                                        -0.8274509803921568f,
                                        -0.4666666666666667f,
                                        -0.6313725490196078f,
                                        -0.5686274509803921f,
                                        -0.5450980392156863f,
                                        -0.5058823529411764f,
                                        0.1450980392156862f,
                                        0.28627450980392166f,
                                        -0.43529411764705883f,
                                        0.9058823529411764f,
                                        0.15294117647058814f,
                                        0.4509803921568627f,
                                        0.03529411764705892f,
                                        -0.7960784313725491f,
                                        0.615686274509804f,
                                        -0.9607843137254902f,
                                        -0.207843137254902f,
                                        0.8431372549019607f,
                                        -0.8509803921568627f,
                                        -0.2313725490196078f,
                                        0.6313725490196078f,
                                        -0.35686274509803917f,
                                        -0.2549019607843137f,
                                        0.1607843137254903f,
                                        0.6470588235294117f,
                                        0.9607843137254901f,
                                        -0.7490196078431373f,
                                        0.7333333333333334f,
                                        0.26274509803921564f,
                                        -0.14509803921568631f,
                                        -0.9843137254901961f,
                                        0.8745098039215686f,
                                        0.8901960784313725f,
                                        -0.607843137254902f,
                                        -0.5294117647058824f,
                                        0.4980392156862745f,
                                        -0.8196078431372549f,
                                        -0.30980392156862746f,
                                        -0.7254901960784313f,
                                        -0.7019607843137254f,
                                        0.9137254901960785f,
                                        0.8588235294117648f,
                                        0.45882352941176463f,
                                        0.9294117647058824f,
                                        0.8980392156862744f,
                                        -0.8117647058823529f,
                                        0.06666666666666665f,
                                        -0.388235294117647f,
                                        -0.4745098039215686f,
                                        -0.3254901960784313f,
                                        -0.17647058823529416f,
                                        -0.0039215686274509665f,
                                        0.24705882352941178f,
                                        0.388235294117647f,
                                        -0.9921568627450981f,
                                        -0.788235294117647f,
                                        0.11372549019607847f,
                                        -0.37254901960784315f,
                                        0.2078431372549019f,
                                        0.21568627450980382f,
                                        0.5058823529411764f,
                                        0.7176470588235293f,
                                        -0.8352941176470589f,
                                        -0.6235294117647059f,
                                        0.2313725490196079f,
                                        -0.9058823529411765f,
                                        0.8823529411764706f,
                                        0.7647058823529411f,
                                        -0.34901960784313724f,
                                        -0.0117647058823529f,
                                        -0.9294117647058824f,
                                        0.30980392156862746f,
                                        0.5215686274509803f,
                                        -0.027450980392156876f,
                                        0.7882352941176471f,
                                        0.17647058823529416f,
                                        0.2784313725490195f,
                                        0.8274509803921568f,
                                        -0.8745098039215686f,
                                        0.2549019607843137f,
                                        -0.6705882352941177f,
                                        0.803921568627451f,
                                        -0.16078431372549018f,
                                        0.607843137254902f,
                                        -0.19999999999999996f,
                                        -0.9372549019607843f,
                                        0.6392156862745098f,
                                        0.5921568627450979f,
                                        -0.24705882352941178f,
                                        -0.10588235294117643f,
                                        -0.5529411764705883f,
                                        0.43529411764705883f,
                                        0.7098039215686274f,
                                        0.4745098039215687f,
                                        -0.615686274509804f,
                                        0.6549019607843136f,
                                        -1.0f,
                                        0.4901960784313726f,
                                        0.5686274509803921f,
                                        -0.1686274509803921f,
                                        0.5529411764705883f,
                                        0.7254901960784315f,
                                        -0.7411764705882353f,
                                        0.9921568627450981f,
                                        0.2705882352941176f,
                                        0.1843137254901961f,
                                        0.5607843137254902f,
                                        0.7568627450980392f,
                                        -0.4274509803921569f,
                                        0.22352941176470598f,
                                        -0.2941176470588235f,
                                        -0.03529411764705881f,
                                        -0.5215686274509803f,
                                        -0.7333333333333334f,
                                        -0.44313725490196076f,
                                        0.09019607843137245f,
                                        -0.9137254901960784f,
                                        -0.33333333333333337f,
                                        -0.8431372549019608f,
                                        0.48235294117647065f,
                                        -0.08235294117647063f,
                                        0.6235294117647059f,
                                        0.3803921568627451f,
                                        0.1215686274509804f,
                                        -0.6941176470588235f,
                                        0.9764705882352942f,
                                        0.3176470588235294f,
                                        -0.22352941176470587f,
                                        0.8196078431372549f,
                                        0.3019607843137255f,
                                        -0.3803921568627451f,
                                        0.019607843137254832f,
                                        0.780392156862745f,
                                        -0.1843137254901961f,
                                        0.3647058823529412f,
                                        0.7960784313725491f,
                                        -0.9764705882352941f,
                                        -0.6549019607843137f,
                                        0.33333333333333326f,
                                        -0.11372549019607847f,
                                        0.8666666666666667f,
                                        -0.7647058823529411f,
                                        -0.4980392156862745f,
                                        0.05882352941176472f,
                                        -0.3176470588235294f,
                                        -0.6784313725490196f,
                                        0.6941176470588235f,
                                        -0.6862745098039216f,
                                        0.3568627450980393f,
                                        0.5764705882352941f,
                                        -0.8901960784313725f,
                                        0.5372549019607844f,
                                        -0.592156862745098f,
                                        0.34901960784313735f,
                                        0.39607843137254894f,
                                        -0.6627450980392157f,
                                        0.13725490196078427f,
                                        0.6784313725490196f,
                                        -0.05882352941176472f,
                                        0.46666666666666656f,
                                        0.8117647058823529f,
                                        -0.28627450980392155f,
                                        0.968627450980392f,
                                        -0.6f,
                                        -0.21568627450980393f,
                                        0.4039215686274509f,
                                        0.9843137254901961f,
                                        0.19215686274509802f,
                                        -0.9215686274509804f,
                                        -0.06666666666666665f,
                                        0.0039215686274509665f,
                                        -0.8823529411764706f,
                                        -0.6392156862745098f,
                                        -0.12941176470588234f,
                                        -0.3019607843137255f,
                                        -0.39607843137254906f,
                                        0.2941176470588236f,
                                        -0.8588235294117648f,
                                        -0.5607843137254902f,
                                        -0.8666666666666667f,
                                        -0.48235294117647054f,
                                        0.6705882352941177f,
                                        -0.803921568627451f,
                                        0.23921568627450984f,
                                        0.7019607843137254f};

ccl_device_inline uint8_t hash_to_byte(uint32_t value)
{
  uint32_t part_1 = value;
  uint32_t part_2 = (value >> 8) * 75;
  uint32_t part_3 = (value >> 16) * 177;
  uint32_t part_4 = (value >> 24) * 233;

  return part_1 ^ part_2 ^ part_3 ^ part_4;
}

ccl_device_inline float hash_to_float(uint32_t value)
{
  return float_lookup_table[hash_to_byte(value)];
}

#ifdef __KERNEL_SSE2__
ccl_device_inline ssef hash_to_float(ssei values)
{
  ssei part_1 = values;
  ssei part_2 = _mm_mullo_epi32(values >> 8, ssei(75));
  ssei part_3 = _mm_mullo_epi32(values >> 16, ssei(177));
  ssei part_4 = _mm_mullo_epi32(values >> 24, ssei(233));

  ssei mixed = part_1 ^ part_2 ^ part_3 ^ part_4;
  mixed &= ssei(0xFF);

  uint32_t indices[4];
  store4i(indices, mixed);
  return ssef(float_lookup_table[indices[0]],
              float_lookup_table[indices[1]],
              float_lookup_table[indices[2]],
              float_lookup_table[indices[3]]);
}
#endif

ccl_device_inline uint hash(uint x, uint y, uint z)
{
  return x;
}

#ifdef __KERNEL_SSE2__
ccl_device_inline ssei hash_sse(const ssei &kx, const ssei &ky, const ssei &kz)
{
  return ssei(0);
}
#endif

#if 0  // unused
ccl_device int imod(int a, int b)
{
  a %= b;
  return a < 0 ? a + b : a;
}

ccl_device uint phash(int kx, int ky, int kz, int3 p)
{
  return hash(imod(kx, p.x), imod(ky, p.y), imod(kz, p.z));
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float floorfrac(float x, int *i)
{
  *i = quick_floor_to_int(x);
  return x - *i;
}
#else
ccl_device_inline ssef floorfrac_sse(const ssef &x, ssei *i)
{
  *i = quick_floor_sse(x);
  return x - ssef(*i);
}
#endif

ccl_device float fade(float t)
{
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
#ifdef __KERNEL_SSE2__
ccl_device_inline ssef fade_sse(ssef t)
{
  ssef a = madd(t, ssef(6.0f), ssef(-15.0f));
  ssef b = madd(t, a, ssef(10.0f));
  return t * t * t * b;
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float nerp(float t, float a, float b)
{
  return (1.0f - t) * a + t * b;
}
#else
ccl_device_inline ssef nerp_sse(const ssef &t, const ssef &a, const ssef &b)
{
  ssef x1 = (ssef(1.0f) - t) * a;
  return madd(t, b, x1);
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float grad(int hash, float x, float y, float z)
{
  // use vectors pointing to the edges of the cube
  int h = hash & 15;
  float u = h < 8 ? x : y;
  float vt = ((h == 12) | (h == 14)) ? x : z;
  float v = h < 4 ? y : vt;
  return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}
#else
ccl_device_inline ssef grad_sse(const ssei &hash, const ssef &x, const ssef &y, const ssef &z)
{
  ssei c1 = ssei(1);
  ssei c2 = ssei(2);

  ssei h = hash & ssei(15);  // h = hash & 15

  sseb case_ux = h < ssei(8);  // 0xffffffff if h < 8 else 0

  ssef u = select(case_ux, x, y);  // u = h<8 ? x : y

  sseb case_vy = h < ssei(4);  // 0xffffffff if h < 4 else 0

  sseb case_h12 = h == ssei(12);  // 0xffffffff if h == 12 else 0
  sseb case_h14 = h == ssei(14);  // 0xffffffff if h == 14 else 0

  sseb case_vx = case_h12 | case_h14;  // 0xffffffff if h == 12 or h == 14 else 0

  ssef v = select(case_vy, y, select(case_vx, x, z));  // v = h<4 ? y : h == 12 || h == 14 ? x : z

  ssei case_uneg = (h & c1) << 31;        // 1<<31 if h&1 else 0
  ssef case_uneg_mask = cast(case_uneg);  // -0.0 if h&1 else +0.0
  ssef ru = u ^ case_uneg_mask;           // -u if h&1 else u (copy float sign)

  ssei case_vneg = (h & c2) << 30;        // 2<<30 if h&2 else 0
  ssef case_vneg_mask = cast(case_vneg);  // -0.0 if h&2 else +0.0
  ssef rv = v ^ case_vneg_mask;           // -v if h&2 else v (copy float sign)

  ssef r = ru + rv;  // ((h&1) ? -u : u) + ((h&2) ? -v : v)
  return r;
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float scale3(float result)
{
  return 0.9820f * result;
}
#else
ccl_device_inline ssef scale3_sse(const ssef &result)
{
  return ssef(0.9820f) * result;
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device_noinline float perlin(float x, float y, float z)
{
  int X;
  float fx = floorfrac(x, &X);
  int Y;
  float fy = floorfrac(y, &Y);
  int Z;
  float fz = floorfrac(z, &Z);

  float u = fade(fx);
  float v = fade(fy);
  float w = fade(fz);

  float result;

  result = nerp(
      w,
      nerp(v,
           nerp(u, grad(hash(X, Y, Z), fx, fy, fz), grad(hash(X + 1, Y, Z), fx - 1.0f, fy, fz)),
           nerp(u,
                grad(hash(X, Y + 1, Z), fx, fy - 1.0f, fz),
                grad(hash(X + 1, Y + 1, Z), fx - 1.0f, fy - 1.0f, fz))),
      nerp(v,
           nerp(u,
                grad(hash(X, Y, Z + 1), fx, fy, fz - 1.0f),
                grad(hash(X + 1, Y, Z + 1), fx - 1.0f, fy, fz - 1.0f)),
           nerp(u,
                grad(hash(X, Y + 1, Z + 1), fx, fy - 1.0f, fz - 1.0f),
                grad(hash(X + 1, Y + 1, Z + 1), fx - 1.0f, fy - 1.0f, fz - 1.0f))));
  float r = scale3(result);

  /* can happen for big coordinates, things even out to 0.0 then anyway */
  return (isfinite(r)) ? r : 0.0f;
}
#else
ccl_device_noinline float perlin(float x, float y, float z)
{
  ssef xyz = ssef(x, y, z, 0.0f);
#  if defined(__KERNEL_SSE41__)
  ssef xyz_low = floor(xyz);
  ssef xyz_high = ceil(xyz);
#  else
  ssef xyz_low = ssef(0);
  ssef xyz_high = ssef(1);
#  endif
  ssef xyz_frac = xyz - xyz_low;
  ssef xyz_fac = fade_sse(xyz_frac);

  float xyz_factors[4];
  store4f(xyz_factors, xyz_fac);

  uint32_t xyz_low_ids[4];
  uint32_t xyz_high_ids[4];
  store4f(xyz_low_ids, xyz_low);
  store4f(xyz_high_ids, xyz_high);

  uint32_t x_low_id = xyz_low_ids[0];
  uint32_t y_low_id = xyz_low_ids[1] * 75;
  uint32_t z_low_id = xyz_low_ids[2] * 177;
  uint32_t x_high_id = xyz_high_ids[0];
  uint32_t y_high_id = xyz_high_ids[1] * 75;
  uint32_t z_high_id = xyz_high_ids[2] * 177;

  uint32_t corner_ll_id = x_low_id ^ y_low_id;
  uint32_t corner_lh_id = x_low_id ^ y_high_id;
  uint32_t corner_hl_id = x_high_id ^ y_low_id;
  uint32_t corner_hh_id = x_high_id ^ y_high_id;

  ssei corner_ids_ll_ll_lh_lh = ssei(corner_ll_id, corner_ll_id, corner_lh_id, corner_lh_id);
  ssei corner_ids_hl_hl_hh_hh = ssei(corner_hl_id, corner_hl_id, corner_hh_id, corner_hh_id);
  ssei z_ids = ssei(z_low_id, z_high_id, z_low_id, z_high_id);

  ssei corner_ids_lll_llh_lhl_lhh = corner_ids_ll_ll_lh_lh ^ z_ids;
  ssei corner_ids_hll_hlh_hhl_hhh = corner_ids_hl_hl_hh_hh ^ z_ids;

  ssef corners_lll_llh_lhl_lhh = hash_to_float(corner_ids_lll_llh_lhl_lhh);
  ssef corners_hll_hlh_hhl_hhh = hash_to_float(corner_ids_hll_hlh_hhl_hhh);

  float result = interpolate_trilinear(xyz_factors[0],
                                       xyz_factors[1],
                                       xyz_factors[2],
                                       corners_lll_llh_lhl_lhh,
                                       corners_hll_hlh_hhl_hhh);
  return result;

  // ssef xyz = ssef(x, y, z, 0.0f);
  // ssei XYZ;

  // ssef fxyz = floorfrac_sse(xyz, &XYZ);

  // ssef uvw = fade_sse(&fxyz);
  // ssef u = shuffle<0>(uvw), v = shuffle<1>(uvw), w = shuffle<2>(uvw);

  // ssei XYZ_ofc = XYZ + ssei(1);
  // ssei vdy = shuffle<1, 1, 1, 1>(XYZ, XYZ_ofc);                       // +0, +0, +1, +1
  // ssei vdz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZ, XYZ_ofc));  // +0, +1, +0, +1

  // ssei h1 = hash_sse(shuffle<0>(XYZ), vdy, vdz);      // hash directions 000, 001, 010, 011
  // ssei h2 = hash_sse(shuffle<0>(XYZ_ofc), vdy, vdz);  // hash directions 100, 101, 110, 111

  // ssef fxyz_ofc = fxyz - ssef(1.0f);
  // ssef vfy = shuffle<1, 1, 1, 1>(fxyz, fxyz_ofc);
  // ssef vfz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyz, fxyz_ofc));

  // ssef g1 = grad_sse(h1, shuffle<0>(fxyz), vfy, vfz);
  // ssef g2 = grad_sse(h2, shuffle<0>(fxyz_ofc), vfy, vfz);
  // ssef n1 = nerp_sse(u, g1, g2);

  // ssef n1_half = shuffle<2, 3, 2, 3>(n1);  // extract 2 floats to a separate vector
  // ssef n2 = nerp_sse(
  //     v, n1, n1_half);  // process nerp([a, b, _, _], [c, d, _, _]) -> [a', b', _, _]

  // ssef n2_second = shuffle<1>(n2);  // extract b to a separate vector
  // ssef result = nerp_sse(
  //     w, n2, n2_second);  // process nerp([a', _, _, _], [b', _, _, _]) -> [a'', _, _, _]

  // ssef r = scale3_sse(result);

  // ssef infmask = cast(ssei(0x7f800000));
  // ssef rinfmask = ((r & infmask) == infmask).m128;  // 0xffffffff if r is inf/-inf/nan else 0
  // ssef rfinite = andnot(rinfmask, r);               // 0 if r is inf/-inf/nan else r
  // return extract<0>(rfinite);
}
#endif

/* perlin noise in range 0..1 */
ccl_device float noise(float3 p)
{
  float r = perlin(p.x, p.y, p.z);
  return 0.5f * r + 0.5f;
}

/* perlin noise in range -1..1 */
ccl_device float snoise(float3 p)
{
  return perlin(p.x, p.y, p.z);
}

/* cell noise */
ccl_device float cellnoise(float3 p)
{
  int3 ip = quick_floor_to_int3(p);
  return bits_to_01(hash(ip.x, ip.y, ip.z));
}

ccl_device float3 cellnoise3(float3 p)
{
  int3 ip = quick_floor_to_int3(p);
#ifndef __KERNEL_SSE__
  float r = bits_to_01(hash(ip.x, ip.y, ip.z));
  float g = bits_to_01(hash(ip.y, ip.x, ip.z));
  float b = bits_to_01(hash(ip.y, ip.z, ip.x));
  return make_float3(r, g, b);
#else
  ssei ip_yxz = shuffle<1, 0, 2, 3>(ssei(ip.m128));
  ssei ip_xyy = shuffle<0, 1, 1, 3>(ssei(ip.m128));
  ssei ip_zzx = shuffle<2, 2, 0, 3>(ssei(ip.m128));
  ssei bits = hash_sse(ip_xyy, ip_yxz, ip_zzx);
  return float3(uint32_to_float(bits) * ssef(1.0f / (float)0xFFFFFFFF));
#endif
}

CCL_NAMESPACE_END
