// EXPECT: valid
// DISASM-NO-MATCH: OpCompositeExtract %int 
// DISASM-MATCH: OpVectorExtractDynamic %float %[0-9]+ %[0-9]+
// DISASM-MATCH: OpAccessChain %_ptr_Private_int %[0-9]+ %[0-9]+
// "::s.v[::a[s.i]]": the index operand is itself a :: chain (file-scope a) whose own subscript
// s.i names the LOCAL s, so every enclosing file-scope hiding is lifted for it. Apple stores 3.
#include <metal_stdlib>
using namespace metal;
struct S { float4 v; int i; }; constant S s = { .v = {1,2,3,4}, .i = 0 }; constant int a[3] = {0,1,2}; constant int b[3] = {0,1,2}; kernel void kern(device float* out [[buffer(0)]]) { S s; s.v = float4(7,8,9,10); s.i = 2; int a = 9; out[0] = ::s.v[::a[s.i]] + a*0; }
