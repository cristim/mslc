// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: %float_5
// A specialization in the namespace of its primary template.
#include <metal_stdlib>
using namespace metal;
namespace n {
template <typename T> float h(T x) { return float(x); }
template <> float h(uchar x) { return float(x) * 5.0f; }
}
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = n::h(uchar(i)) + n::h(i); }
