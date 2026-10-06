// EXPECT: error with empty parentheses declares a function
#include <metal_stdlib>
using namespace metal;
constexpr sampler S();
fragment float4 f() { return float4(0.0); }
