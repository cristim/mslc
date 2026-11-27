// EXPECT: error assigning to a comma expression is not lowered yet
// Apple accepts this (the comma yields an lvalue, so t is assigned); mslc
// refuses it loudly rather than lowering the store through a value.
kernel void comma_assignment_target_rejected(device float *o [[buffer(0)]]) {
  int n = 0, t = 0;
  (n, t) = 3;
  o[0] = float(t);
}
