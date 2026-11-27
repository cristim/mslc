// EXPECT: valid
// DISASM: OpVectorExtractDynamic
// "::v[v]": a file-scope float4 indexed by a local int of the same name.
#include <metal_stdlib>
using namespace metal;
constant float4 v = {1,2,3,4}; kernel void kern(device float* out [[buffer(0)]]) { int v = 1; out[0] = ::v[v]; }
