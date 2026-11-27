// EXPECT: valid
// DISASM: OpConvertSToF
// DISASM: OpFunctionCall %float
// Refs #341: "f<float>(1)" names the instance, and the int argument converts to its parameter. It
// would otherwise read as a chain of comparisons.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f<float>(1); }
