// EXPECT: error fmod takes 2 arguments
kernel void math_fmod_arity_rejected(device float *o [[buffer(0)]], constant float *a [[buffer(1)]]) {
  o[0] = fmod(a[0]);
}
