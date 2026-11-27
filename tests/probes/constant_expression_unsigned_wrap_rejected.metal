// EXPECT: error a constant expression that mixes an unsigned operand with a negative value or subtracts below zero
// 3u - 5u wraps as an unsigned subtraction, which the evaluator does not model.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]]) { float a[3u - 5u]; out[0] = 1; }
