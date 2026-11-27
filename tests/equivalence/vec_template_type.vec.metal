// vec<T, N> spelled out; vec_template_type.named.metal writes the same vectors by name.
#include <metal_stdlib>
using namespace metal;
struct P { vec<float, 2> a; metal::vec<int, 3> b; vec<half, 4> c; vec<uchar, 4> d; };
static vec<float, 3> scaled(vec<float, 3> v, float s) { return v * s; }
kernel void k(device const P* in [[buffer(0)]], device vec<float, 4>* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  vec<float, 2 * 2> f = vec<float, 4>(in[i].a, 1.0f, 2.0f);
  vec<uint, 2> u = uint2(i, i + 1);
  vec<bool, 2> b = bool2(u.x > 3, u.y > 3);
  vec<float, 3> s = scaled(float3(in[i].b), 0.5f);
  vec<short, 2> h = short2(in[i].d.xy);
  out[i] = f + float4(float(u.y), b.y ? 1.0f : 0.0f, s.z, float(in[i].c.w) + float(h.y));
}
