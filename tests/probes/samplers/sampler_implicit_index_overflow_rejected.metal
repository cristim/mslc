// EXPECT: error no 'sampler' resource location is available
// Apple: "no 'sampler' resource location available for 't16'".
#include <metal_stdlib>
using namespace metal;
fragment float4 f(sampler t0, sampler t1, sampler t2, sampler t3, sampler t4, sampler t5, sampler t6, sampler t7, sampler t8, sampler t9, sampler t10, sampler t11, sampler t12, sampler t13, sampler t14, sampler t15, sampler t16) { return float4(0.0); }
