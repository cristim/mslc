// EXPECT: valid
// The corpus use (MDPDefaultLit): a const array of float3 indexed by a loop counter.
kernel void local_array_vectors_dynamic_index(device float *o [[buffer(0)]]) {
  const float3 d[3] = { float3(3.0, 3.0, 3.0), float3(0.0, -3.0, -1.0), float3(-2.0, 0.0, 0.0) };
  float sum = 0.0;
  for (int i = 0; i < 3; i++) { float3 l = normalize(d[i]); sum += l.x + l.y + l.z; }
  o[0] = sum;
}
