// EXPECT: error declared const
// Apple: cannot assign to variable with const-qualified type const int.
kernel void auto_local_const_assignment_rejected(device float *o [[buffer(0)]]) {
  const auto x = 1;
  x = 2;
  o[0] = x;
}
