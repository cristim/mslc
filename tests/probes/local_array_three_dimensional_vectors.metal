// EXPECT: valid
// DISASM: OpTypeArray %_arr__arr_float_uint_2_uint_2_0 %uint_2_1
// Three dimensions, and a component of a vector element reached through two indices.
// Lavapipe readback against Apple's GPU: d[1][1][1] is 9, v[1][0].y is 6.
kernel void local_array_three_dimensional_vectors(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float d[2][2][2] = {};
  d[i + 1][1][i + 1] = 9.0;
  float2 v[2][2] = {{float2(1.0, 2.0), float2(3.0, 4.0)}, {float2(5.0, 6.0), float2(7.0, 8.0)}};
  o[0] = d[1][1][1];
  o[1] = v[i + 1][0].y;
  v[1][1].x = 11.0;
  o[2] = v[1][1].x + v[1][1].y;
}
