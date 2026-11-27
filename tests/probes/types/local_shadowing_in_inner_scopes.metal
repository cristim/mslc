// EXPECT: valid
// A name may be declared again in an inner block, in sibling blocks, in the
// blocks of different cases, and by two loops one after the other.
kernel void local_shadowing_in_inner_scopes(device float *o [[buffer(0)]], uint p [[thread_position_in_grid]]) {
  float a = 1.0;
  { float a = 2.0; o[0] = a; }
  { float a = 3.0; o[1] = a; }
  switch (p) {
    case 0: { float b = 1.0; o[2] = b; break; }
    case 1: { float b = 2.0; o[2] = b; break; }
  }
  for (int i = 0; i < 2; i++) { o[3] = a; }
  for (int i = 0; i < 2; i++) { o[4] = a; }
}
