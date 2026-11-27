// EXPECT: error a constant's initialiser has to be a value that can be worked out from the declarations before it
// A vector operand to as_type in a file-scope constant's initialiser is refused rather than folded.
#include <metal_stdlib>
using namespace metal;
constant ulong kOneBits = as_type<ulong>(uint2(1u, 2u));
kernel void k(device ulong* out [[buffer(0)]]) { out[0] = kOneBits; }
