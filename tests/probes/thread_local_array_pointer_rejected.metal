// EXPECT: error an array of references, pointers or resources is not lowered yet
// A thread-qualified array of pointers is still not lowered: only the array of values is.
kernel void thread_local_array_pointer_rejected(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  thread float *p[2];
  o[i] = 1.0;
}
