// EXPECT: valid
// DISASM: OpCompositeConstruct
// DISASM-MATCH: OpCompositeExtract %float %[_0-9a-zA-Z]+ 2
#include <metal_stdlib>
kernel void k(device float *o [[buffer(0)]]){metal::float3 v=metal::float3(o[1],o[1]+1.0f,o[1]+2.0f);o[0]=v.z;}
