// EXPECT: error a component of a vector, which a non-const reference cannot name
// Apple refuses `float &r = v.y;` for a vector component (see local_const_reference_to_vector_component_copies).
kernel void local_reference_to_vector_component_rejected(device float *o [[buffer(0)]]) {
  float4 v = float4(1.0);
  float &r = v.y;
  o[0] = r;
}
