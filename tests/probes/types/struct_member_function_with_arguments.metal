// EXPECT: valid
// DISASM: OpFunctionCall
struct S { float x; float scaled(float k, float bias) const { return x * k + bias; } };
kernel void k(device float *out [[buffer(0)]]) { S s; s.x = 2.0; out[0] = s.scaled(3.0, 1.0); }
