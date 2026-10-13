// EXPECT: error which has a constructor, needs a value for every element
// Apple runs the default constructor for each element (s[1].x is 3); zero-filling would read 0.
struct S { float x; S() { x = 3.0; } };
kernel void local_array_constructor_struct_unfilled_rejected(device float *o [[buffer(0)]]) {
  S s[2];
  o[0] = s[1].x;
}
