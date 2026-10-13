// EXPECT: valid
struct S { float x; S() { x = 3.0; } };
kernel void local_array_constructor_struct_filled(device float *o [[buffer(0)]]) {
  S s[2] = { S(), S() };
  o[0] = s[1].x;
}
