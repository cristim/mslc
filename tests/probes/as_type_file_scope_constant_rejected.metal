// EXPECT: error a constant's initialiser has to be a value that can be worked out from the declarations before it
// as_type in a file-scope constant's initialiser is refused rather than folded: the constant folder
// knows conversions, and reading as_type as one would give the converted value instead of the bits.
#include <metal_stdlib>
using namespace metal;
constant uint kOneBits = as_type<uint>(1.0f);
kernel void k(device uint* out [[buffer(0)]]) { out[0] = kOneBits; }
