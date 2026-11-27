// EXPECT: valid
// DISASM: OpFunctionCall
struct S { float x; float y; float z; S(float a) : z(a) { x = 1.0; y = z + x; } };
kernel void k(device float *out [[buffer(0)]]) { S s(4.0); out[0] = s.x + s.y + s.z; }
