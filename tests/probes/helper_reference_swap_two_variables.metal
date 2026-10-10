// EXPECT: valid
// Two distinct variables, each passed as itself, are the usual swap.
void swap2(thread float &a, thread float &b) { float t = a; a = b; b = t; }
kernel void helper_reference_swap_two_variables(device float *o [[buffer(0)]]) {
  float x = 1.0;
  float y = 2.0;
  swap2(x, y);
  o[0] = x;
  o[1] = y;
}
