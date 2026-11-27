// EXPECT: valid
// DISASM: OpVectorExtractDynamic
// "::s.v[s.i]": only the root is the file-scope s; the index s.i is the local's (2, stored
// at run time), so the extraction is dynamic. Apple stores 3 here.
#include <metal_stdlib>
using namespace metal;
struct S { float4 v; int i; }; constant S s = { .v = {1,2,3,4}, .i = 0 }; kernel void kern(device float* out [[buffer(0)]]) { S s; s.v = float4(7,8,9,10); s.i = 2; out[0] = ::s.v[s.i]; }
