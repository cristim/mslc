// EXPECT: valid
constant float K[] = {1.0, 2.0, 3.0};
kernel void file_scope_constant_array_deduced_length(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  o[0] = K[i + 1];
}
