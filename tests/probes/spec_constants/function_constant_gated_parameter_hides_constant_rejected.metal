// EXPECT: error on a declaration other than a file-scope constant, it names a program-scope constant
// A parameter named like the function constant hides it, and a parameter is not a program-scope constant: Apple's
// compiler refuses its name as the argument (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
constant bool g [[function_constant(0)]];
kernel void k(device float* g [[buffer(0)]], device float* a [[buffer(1), function_constant(g)]]) { g[0] = 1; }
