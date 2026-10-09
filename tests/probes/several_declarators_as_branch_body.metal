// EXPECT: valid
// A declaration with several names is one statement, so it can be the body of an if.
kernel void several_declarators_as_branch_body(device float *o [[buffer(0)]]) {
  float a = 1.0;
  if (a > 0.0) float c = 1.0, d = 2.0;
  o[0] = a;
}
