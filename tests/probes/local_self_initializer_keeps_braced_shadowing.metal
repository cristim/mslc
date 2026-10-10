// EXPECT: valid
// Shadowing a same-named local in an inner brace still shadows it, after the
// self-reference fix installed a binding at the point of declaration. `value`
// is 7 in the outer block and 2 in the inner one, and the store into `out`
// after the inner block reads the outer 7 again.
kernel void braced_shadow_still_shadows_outer(device int *out [[buffer(0)]]) {
  int value = 7;
  {
    int value = 2;
    out[0] = value;
  }
  out[0] = value;
}