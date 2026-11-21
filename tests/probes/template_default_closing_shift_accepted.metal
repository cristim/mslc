// EXPECT: valid
// DISASM-NOT: OpFunctionCall
// Refs #341: the ">>" that ends a default's own template-id and the parameter list is two closers.
#include <metal_stdlib>
using namespace metal;
template <typename A, typename B = vec<A, 2>> B pair(A a);
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
