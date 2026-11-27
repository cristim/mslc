// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpTypeArray %_arr_v2float_uint_2 %uint_2
// thread on an array of arrays of vectors and on an array of structs.
struct Pair { float x; int y; };
kernel void thread_local_array_two_dimensional_struct(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  thread float2 m[2][2];
  thread Pair p[2] = {{1.0, 2}, {3.0, 4}};
  m[1][0] = float2(3.0);
  o[i] = m[1][i & 1].x + p[i & 1].x;
}
