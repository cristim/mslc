// EXPECT: error needs the index of a function constant on a file-scope constant and the name of a program-scope constant anywhere else
// A parameter is not a program-scope constant.
#include <metal_stdlib>
using namespace metal;
constant bool g [[function_constant(0)]];
kernel void k(device float* out [[buffer(0)]], device float* a [[buffer(1), function_constant(out)]]) { out[0] = 1; }
