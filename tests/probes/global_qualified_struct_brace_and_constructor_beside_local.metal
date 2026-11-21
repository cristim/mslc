// EXPECT: valid
// DISASM: OpCompositeExtract %float
// DISASM: OpFMul
// "::T{5}.v" builds the struct T though a local float T hides the tag; T*10 is the local.
#include <metal_stdlib>
using namespace metal;
struct T{float v;}; kernel void kern(device float* out [[buffer(0)]]) { float T=2; out[0]=::T{5}.v + T*10; }
