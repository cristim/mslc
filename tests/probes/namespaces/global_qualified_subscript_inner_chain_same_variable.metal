// EXPECT: valid
// DISASM-MATCH: OpVectorExtractDynamic %float %[0-9]+ %[0-9]+
// "::s.v[::s.i]" indexes the file-scope vector by the file-scope member (0), while
// "::s.v[s.i]" uses the local's (2): the inner chain naming the same variable is still ::.
#include <metal_stdlib>
using namespace metal;
struct S { float4 v; int i; }; constant S s = { .v = {1,2,3,4}, .i = 0 }; constant int a[3] = {0,1,2}; constant int b[3] = {0,1,2}; kernel void kern(device float* out [[buffer(0)]]) { S s; s.v = float4(7,8,9,10); s.i = 2; out[0] = ::s.v[::s.i] + ::s.v[s.i]*10; }
