// EXPECT: error is a component of a vector
// Apple refuses v[1] and m[1][0] for a reference parameter, as it does v.y; mslc passed them through a copy.
void f(thread float &x) { x = 1.0f; }
kernel void helper_reference_vector_element_rejected(device float *o [[buffer(0)]]) {
  float3 v = float3(0.0f);
  f(v[1]);
  o[0] = v.y;
}
