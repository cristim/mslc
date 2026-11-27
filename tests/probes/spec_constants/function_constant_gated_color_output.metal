// EXPECT: valid
// DISASM-MATCH: OpDecorate %[0-9a-z_]+ Location 1
//
// A colour output gated by a function constant is declared either way; with the constant false the pipeline has no
// attachment there and what is written to it is dropped.
#include <metal_stdlib>
using namespace metal;
constant bool hasSecond [[function_constant(0)]];
struct Out { float4 first [[color(0)]]; float4 second [[color(1), function_constant(hasSecond)]]; };
fragment Out f() { Out o; o.first = 1; if (hasSecond) { o.second = 2; } return o; }
