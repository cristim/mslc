// EXPECT: error a statement expression "({ ... })" is lowered only as a statement of its own
// Followed by more than ";", the statement expression is an operand, which is not lowered even where its value is dropped.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { ({ out[i] = i; }), out[i] += 1u; }
