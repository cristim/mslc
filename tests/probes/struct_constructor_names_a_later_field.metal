// EXPECT: valid
// DISASM: OpFunctionCall
//
// A constructor may name a member that is declared after it.
struct S { S(float a) : y(a) { x = a; } float x; float y; };
kernel void k(device float *out [[buffer(0)]]) { S s(4.0); out[0] = s.x + s.y; }
