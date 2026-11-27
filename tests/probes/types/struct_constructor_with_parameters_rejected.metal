// EXPECT: error no constructor of "S" takes 0 arguments
// Apple: no matching constructor for initialization of S.
struct S { float x; S(float a) { x = a; } };
kernel void struct_constructor_with_parameters_rejected(device float *out [[buffer(0)]]) { S s; out[0] = s.x; }
