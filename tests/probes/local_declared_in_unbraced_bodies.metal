// EXPECT: valid
// The body of an if, else or while that is not a block is a scope of its own, so
// the same name can be declared in each and again after them.
kernel void local_declared_in_unbraced_bodies(device float *o [[buffer(0)]], uint p [[thread_position_in_grid]]) {
  if (p == 1) float a = 1.0; else if (p == 2) float a = 2.0; else float a = 3.0;
  while (p < 1) float a = 1.0;
  float a = 4.0;
  o[0] = a;
}
