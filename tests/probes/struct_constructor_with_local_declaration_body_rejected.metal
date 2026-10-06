// EXPECT: error a constructor with a body is not lowered yet
// A body holding a declaration must not be read as a field.
struct S { float x; S() { float y; } };
kernel void struct_constructor_with_local_declaration_body_rejected(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
