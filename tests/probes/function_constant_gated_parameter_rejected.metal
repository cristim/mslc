// EXPECT: error on a declaration other than a file-scope constant, it names a program-scope constant
// A parameter is not a program-scope constant.
#include <metal_stdlib>
using namespace metal;
constant bool g [[function_constant(0)]];
kernel void k(device float* out [[buffer(0)]], device float* a [[buffer(1), function_constant(out)]]) { out[0] = 1; }
