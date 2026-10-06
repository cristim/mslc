// EXPECT: error a constructor with parameters is not lowered yet
struct S { float x; S(float a) {} };
kernel void struct_constructor_with_parameters_rejected(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
