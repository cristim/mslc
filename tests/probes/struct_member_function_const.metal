// EXPECT: valid
// DISASM: OpFunctionCall
struct S { float x; float y; float sum() const { return x + y; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 1.0; s.y = 2.0; out[0] = s.sum(); }
