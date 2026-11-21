// EXPECT: error a pointer is not a value mslc can declare
// A thread pointer local is still not lowered; only the value locals drop the qualifier.
kernel void thread_local_pointer_rejected(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float x = 1;
  thread float *p = &x;
  o[i] = *p;
}
