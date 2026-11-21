// EXPECT: valid
// DISASM: OpIAdd %int %int_1 %int_2
enum E { A = 1, B = 2 };
kernel void k(device float* out [[buffer(0)]]) { float y{A + B}; out[0] = y; }
