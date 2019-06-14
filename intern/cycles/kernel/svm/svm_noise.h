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

static uint8_t lookup_table[256] = {
    229, 239, 214, 247, 11,  206, 113, 235, 250, 44,  234, 221, 197, 147, 49,  213, 36,  249, 91,
    7,   173, 176, 172, 245, 246, 211, 17,  146, 6,   107, 59,  244, 120, 134, 156, 215, 243, 119,
    240, 190, 153, 159, 200, 186, 188, 73,  0,   64,  224, 116, 208, 139, 136, 124, 154, 219, 68,
    30,  83,  60,  118, 241, 37,  180, 112, 102, 148, 131, 227, 171, 40,  43,  1,   130, 90,  192,
    210, 97,  184, 104, 222, 54,  81,  255, 72,  128, 62,  252, 205, 19,  185, 46,  48,  58,  207,
    13,  168, 79,  162, 248, 82,  26,  32,  178, 63,  138, 135, 157, 253, 57,  67,  3,   89,  151,
    110, 105, 8,   14,  170, 24,  129, 152, 99,  169, 177, 28,  47,  141, 108, 55,  125, 114, 181,
    132, 100, 161, 198, 98,  15,  56,  50,  21,  16,  22,  201, 144, 42,  230, 194, 212, 187, 109,
    127, 175, 231, 31,  87,  189, 254, 123, 143, 25,  27,  106, 76,  84,  103, 77,  18,  237, 203,
    78,  12,  61,  122, 232, 75,  33,  140, 251, 92,  199, 149, 196, 41,  195, 216, 9,   158, 193,
    23,  183, 94,  86,  85,  69,  38,  35,  2,   74,  150, 96,  53,  45,  137, 95,  111, 163, 117,
    164, 182, 126, 238, 155, 179, 191, 225, 228, 145, 93,  80,  142, 66,  174, 52,  226, 10,  4,
    217, 71,  39,  223, 242, 88,  165, 166, 209, 133, 167, 34,  65,  220, 29,  202, 204, 115, 233,
    70,  5,   20,  160, 101, 236, 218, 121, 51};

ccl_device uint hash(uint kx, uint ky, uint kz)
{
  uint32_t p32_1 = kx * 75;
  uint32_t p32_2 = ky * 47;
  uint32_t p32_3 = kz * 115;
  uint32_t mixed32 = p32_1 ^ p32_2 ^ p32_3;

  uint8_t b1 = mixed32 >> 0;
  uint8_t b2 = mixed32 >> 8;
  uint8_t b3 = mixed32 >> 16;
  uint8_t b4 = mixed32 >> 24;

  b1 *= 103;
  b2 *= 233;
  b3 *= 241;
  b4 *= 215;

  uint8_t mixed = b1 ^ b2 ^ b3 ^ b4;
  return lookup_table[mixed];
}

#ifdef __KERNEL_SSE2__
ccl_device_inline ssei hash_sse(const ssei &kx, const ssei &ky, const ssei &kz)
{
  uint kx_[4];
  uint ky_[4];
  uint kz_[4];

  store4i(kx_, kx);
  store4i(ky_, ky);
  store4i(kz_, kz);

  uint result[4];
  result[0] = hash(kx_[0], ky_[0], kz_[0]);
  result[1] = hash(kx_[1], ky_[1], kz_[1]);
  result[2] = hash(kx_[2], ky_[2], kz_[2]);
  result[3] = hash(kx_[3], ky_[3], kz_[3]);

  return load4i(result);
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

#ifndef __KERNEL_SSE2__
ccl_device float fade(float t)
{
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
#else
ccl_device_inline ssef fade_sse(const ssef *t)
{
  ssef a = madd(*t, ssef(6.0f), ssef(-15.0f));
  ssef b = madd(*t, a, ssef(10.0f));
  return ((*t) * (*t)) * ((*t) * b);
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
  ssei XYZ;

  ssef fxyz = floorfrac_sse(xyz, &XYZ);

  ssef uvw = fade_sse(&fxyz);
  ssef u = shuffle<0>(uvw), v = shuffle<1>(uvw), w = shuffle<2>(uvw);

  ssei XYZ_ofc = XYZ + ssei(1);
  ssei vdy = shuffle<1, 1, 1, 1>(XYZ, XYZ_ofc);                       // +0, +0, +1, +1
  ssei vdz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZ, XYZ_ofc));  // +0, +1, +0, +1

  ssei h1 = hash_sse(shuffle<0>(XYZ), vdy, vdz);      // hash directions 000, 001, 010, 011
  ssei h2 = hash_sse(shuffle<0>(XYZ_ofc), vdy, vdz);  // hash directions 100, 101, 110, 111

  ssef fxyz_ofc = fxyz - ssef(1.0f);
  ssef vfy = shuffle<1, 1, 1, 1>(fxyz, fxyz_ofc);
  ssef vfz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyz, fxyz_ofc));

  ssef g1 = grad_sse(h1, shuffle<0>(fxyz), vfy, vfz);
  ssef g2 = grad_sse(h2, shuffle<0>(fxyz_ofc), vfy, vfz);
  ssef n1 = nerp_sse(u, g1, g2);

  ssef n1_half = shuffle<2, 3, 2, 3>(n1);  // extract 2 floats to a separate vector
  ssef n2 = nerp_sse(
      v, n1, n1_half);  // process nerp([a, b, _, _], [c, d, _, _]) -> [a', b', _, _]

  ssef n2_second = shuffle<1>(n2);  // extract b to a separate vector
  ssef result = nerp_sse(
      w, n2, n2_second);  // process nerp([a', _, _, _], [b', _, _, _]) -> [a'', _, _, _]

  ssef r = scale3_sse(result);

  ssef infmask = cast(ssei(0x7f800000));
  ssef rinfmask = ((r & infmask) == infmask).m128;  // 0xffffffff if r is inf/-inf/nan else 0
  ssef rfinite = andnot(rinfmask, r);               // 0 if r is inf/-inf/nan else r
  return extract<0>(rfinite);
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
