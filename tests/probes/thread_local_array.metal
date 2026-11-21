// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_4
// DISASM-MATCH: OpVariable %_ptr_Function__arr_float_uint_4 Function
// A local array may carry the thread qualifier (Apple accepts it); it is the function's own
// storage, the same as an unqualified one. Dynamic index, initialiser list and const all apply.
kernel void thread_local_array(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  thread float a[4] = {1.0, 2.0};
  thread const float c[2] = {5.0, 6.0};
  a[i & 3] += c[i & 1];
  o[i] = a[(i + 1) & 3];
}
