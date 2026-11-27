// EXPECT: valid
// A local of the same name hides the file-scope array, as in C++: the local has three elements.
constant float K[2] = {1.0, 2.0};
kernel void file_scope_constant_array_shadowed_by_local(device float *o [[buffer(0)]]) {
  float K[3] = {5.0, 6.0, 7.0};
  o[0] = K[2];
}
