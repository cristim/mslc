// EXPECT: valid
// DISASM: OpFunctionCall
// A body holding a declaration is a body, not a field.
struct S { float x; S() { float y = 3.0; x = y; } };
kernel void struct_constructor_with_local_declaration_body_is_lowered(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
