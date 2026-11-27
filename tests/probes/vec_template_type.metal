// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM-MATCH: OpTypeVector %int 3
// vec<T, N> is the vector of N T: as a local, a parameter, a return type, a struct field in a buffer
// (laid out as the named vector), with the namespace and with a constant expression as N.
#include <metal_stdlib>
using namespace metal;
struct P { vec<float, 2> a; metal::vec<int, 3> b; vec<half, 4> c; };
static vec<float, 3> scaled(vec<float, 3> v, float s) { return v * s; }
kernel void k(device const P* in [[buffer(0)]], device float* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  vec<float, 2 * 2> f = float4(in[i].a, 1.0f, 2.0f);
  vec<uint, 2> u = uint2(i, i + 1);
  vec<bool, 2> b = bool2(u.x > 3, u.y > 3);
  vec<float, 3> s = scaled(float3(in[i].b), 0.5f);
  out[i] = f.x + f.w + float(u.y) + (b.y ? 1.0f : 0.0f) + s.z + float(in[i].c.w)
    + sizeof(P) + sizeof(vec<float, 3>) + sizeof(vec<short, 2>);
}
