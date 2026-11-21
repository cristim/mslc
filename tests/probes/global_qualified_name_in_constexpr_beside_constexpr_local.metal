// EXPECT: valid
// DISASM: OpIMul %int %int_3 %int_2
// "::n * 2" reads the file-scope n (3) though a constexpr local n (5) hides it: Apple stores 6.
#include <metal_stdlib>
constant int n = 3; kernel void kern(device int* out [[buffer(0)]]) { constexpr int n = 5; constexpr int m = ::n * 2; out[0] = m; }
