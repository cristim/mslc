// EXPECT: valid
// DISASM: OpFunctionCall
struct S { float x; static float twice(float a) { return a * 2.0; } };
kernel void k(device float *out [[buffer(0)]]) { out[0] = S::twice(4.0); }
