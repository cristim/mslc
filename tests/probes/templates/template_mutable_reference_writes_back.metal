// EXPECT: valid
// DISASM: OpFunctionParameter %_ptr_Function_int
// Refs #341: thread T& with an int lvalue instantiates T = int, and the callee writes the caller's variable.
#include <metal_stdlib>
using namespace metal;
template <typename T> void bump(thread T& x) { x = x + T(1); }
kernel void k(device int* out [[buffer(0)]]) { int v = 1; bump(v); out[0] = v; }
