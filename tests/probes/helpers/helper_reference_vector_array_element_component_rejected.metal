// EXPECT: error is a component of a vector
void f(thread float &x) { x = 1.0f; }
kernel void helper_reference_vector_array_element_component_rejected(device float *o [[buffer(0)]]) {
  float4 av[2] = {float4(0.0f), float4(0.0f)};
  f(av[1][2]);
  o[0] = av[1].z;
}
