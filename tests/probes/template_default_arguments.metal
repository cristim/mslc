// EXPECT: valid
// DISASM: OpFunctionCall %int
// DISASM: OpFunctionCall %float
// DISASM: %int_7
// Refs #341: a default type (T = float) and a default constant (N = 2) are used when the call gives
// neither: f() is f<float, 2>, f<int>() is f<int, 2>, and f<int, 7>() is a third.
#include <metal_stdlib>
using namespace metal;
template <typename T = float, int N = 2> T f() { return T(N); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f() + float(f<int>()) + float(f<int, 7>()); }
