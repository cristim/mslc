// EXPECT: error redefinition of "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
constexpr sampler S(filter::linear);
fragment float4 f() { return float4(0.0); }
