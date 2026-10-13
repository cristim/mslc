// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_4
// A shorter initialiser zero-fills the rest (lavapipe readback: a[0] + a[1] is 3, a[2] + a[3] is 0).
kernel void local_array_initialised(device float *o [[buffer(0)]]) {
  float a[4] = {1.0, 2.0};
  o[0] = a[0] + a[1];
  o[1] = a[2] + a[3];
}
