// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// An explicit specialization of a template on vec<I, 4> deduces I from the vector's component type, as a call
// does: vec<float, 4> specializes it for I = float. wf(float4) calls the specialization (i + 2000), wf(int4) the
// primary (i + 1000), so on an RTX 3060 threads 0 to 2 write 12000, 12011 and 12022.
#include <metal_stdlib>
using namespace metal;
template <typename I> inline float wf(vec<I, 4> d) { return float(d.x) + 1000.0f; }
template <> inline float wf(vec<float, 4> d) { return d.x + 2000.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = wf(float4(float(i))) + wf(int4(int(i))) * 10.0f; }
