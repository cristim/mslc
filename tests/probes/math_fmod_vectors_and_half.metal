// EXPECT: valid
kernel void math_fmod_vectors_and_half(device float4 *o [[buffer(0)]], constant float4 *a [[buffer(1)]], constant half *h [[buffer(2)]], device half *oh [[buffer(3)]]) {
  o[0] = fmod(a[0], a[1]);
  o[1] = fmod(a[0], 3.0);
  oh[0] = fmod(h[0], h[1]);
}
