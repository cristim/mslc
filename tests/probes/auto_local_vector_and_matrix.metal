// EXPECT: valid
// DISASM-MATCH: OpTypeMatrix
kernel void auto_local_vector_and_matrix(device float *o [[buffer(0)]]) {
  auto v = float3(1, 2, 3) * 2.0f;
  auto iv = int2(7, 9) / 2;
  auto m = float2x2(float2(1, 2), float2(3, 4));
  auto m2 = m * m;
  o[0] = v.z + iv.x + m2[1][1];
}
