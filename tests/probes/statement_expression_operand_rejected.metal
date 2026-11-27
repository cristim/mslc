// EXPECT: error a statement expression "({ ... })" whose value is used is not lowered
// Followed by more than ";", the statement expression is an operand, whose value is used.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { ({ out[i] = i; }), out[i] += 1u; }
