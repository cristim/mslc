// EXPECT: valid
// DISASM: OpFunctionCall
// A constructor body that assigns a field is lowered to a helper function and called, not dropped.
struct S { float x; S() { x = 2.0; } };
kernel void struct_constructor_with_body_is_lowered(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
