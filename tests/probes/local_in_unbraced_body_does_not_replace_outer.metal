// EXPECT: valid
// The outer a is read after an unbraced body declared another a; verified by a
// lavapipe readback that o[0] is the outer 1.0 and not the 2.0 or 3.0 declared in the branches.
kernel void local_in_unbraced_body_does_not_replace_outer(device float *o [[buffer(0)]], uint p [[thread_position_in_grid]]) {
  float a = 1.0;
  if (p) float a = 2.0; else float a = 3.0;
  o[0] = a;
}
