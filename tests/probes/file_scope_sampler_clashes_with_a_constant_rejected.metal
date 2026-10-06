// EXPECT: error redefinition of "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
constant float S = 1.0;
fragment float4 f() { return float4(0.0); }
