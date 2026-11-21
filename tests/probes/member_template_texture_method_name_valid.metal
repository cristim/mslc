// EXPECT: valid
// Refs #341: a member template named like a texture method does not take the texture's own call.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> float read(U x) const { return a + float(x); } };
kernel void k(texture2d<float, access::read> t [[texture(0)]], device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{1.0f}; out[i] = s.read(2) + t.read(uint2(0, 0)).x; }
