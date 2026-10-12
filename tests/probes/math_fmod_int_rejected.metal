// EXPECT: error fmod takes float or half scalars or vectors
// Apple: "call to fmod is ambiguous" for integer arguments.
kernel void math_fmod_int_rejected(device float *o [[buffer(0)]], constant int *i [[buffer(1)]]) {
  o[0] = fmod(i[0], 2);
}
