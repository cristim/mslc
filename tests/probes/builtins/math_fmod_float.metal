// EXPECT: valid
// DISASM-MATCH: OpFRem
// fmod keeps the sign of the dividend (Apple GPU: fmod(-5.5, 2) = -1.5, fmod(5.5, -2) = 1.5) and
// is NaN for an infinite divisor, which is what OpFRem gives; lavapipe readback matches the GPU.
kernel void math_fmod_float(device float *o [[buffer(0)]], constant float *a [[buffer(1)]], constant float *b [[buffer(2)]]) {
  for (int i = 0; i < 16; i++) o[i] = fmod(a[i], b[i]);
}
