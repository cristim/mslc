// EXPECT: valid
// DISASM: OpFunctionCall
// An initialiser list is lowered into the constructor helper.
struct S { float x; S() : x(1.0) {} };
kernel void struct_constructor_with_init_list_is_lowered(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
