// EXPECT: error a constructor with an initialiser list is not lowered yet
struct S { float x; S() : x(1.0) {} };
kernel void struct_constructor_with_init_list_rejected(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
