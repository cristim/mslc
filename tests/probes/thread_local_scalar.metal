// EXPECT: valid
// thread on a scalar local, with = , parentheses and braces.
kernel void thread_local_scalar(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  thread float a = 1;
  thread float b(2);
  thread int c{3};
  thread auto d = 4.0f;
  o[i] = a + b + c + d;
}
