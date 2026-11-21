// EXPECT: valid
// Refs #341: a field named like a class template is the field in a member: "Box < q" compares.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
struct H { float Box; float g(float q) const { return (Box < q) ? 1.0f : 2.0f; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ H h; h.Box = 1.0f; out[i] = h.g(3.0f); }
