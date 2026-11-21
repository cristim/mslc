// EXPECT: valid
// DISASM-CAPTURE: OpFunctionCall %int (%[0-9]+)
// DISASM-MATCH-CAPTURED: OpFunctionCall %int $1 .*OpFunctionCall %int $1 
// DISASM-NO-MATCH: OpFunctionCall %int (%[0-9]+) .*OpFunctionCall %float \1
// Refs #341: the two int calls share one OpFunction, and the float call is another: an instance is
// made once per distinct argument list.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x + x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = float(f(1) + f(2)) + f(1.0f); }
