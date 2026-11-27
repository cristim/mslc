// EXPECT: error on a parameter needs the name of a program-scope constant
// Apple's compiler says the argument must be a program scope constant variable (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
constant bool g [[function_constant(0)]];
kernel void k(device float* out [[buffer(0)]], device float* a [[buffer(1), function_constant(0)]]) { out[0] = 1; }
