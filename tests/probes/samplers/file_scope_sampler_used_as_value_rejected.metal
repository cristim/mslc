// EXPECT: error is a sampler, which mslc uses as the receiver
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f() { float x = S; return float4(x); }
