// EXPECT: valid
// A first length left out, rank 3, and a const parameter. Lavapipe readback against Apple's GPU:
// sum3 is 16, v[1].y is 5, rd(cc, 2) is 6.
float sum3(float m[][2][2]) { return m[1][1][0] + m[0][1][1] + m[1][0][0]; }
void setv(float3 v[2], int i) { v[i].y = 5.0; }
float rd(const float m[3], int i) { return m[i]; }
kernel void helper_array_parameter_three_dimensional_vectors(device float *o [[buffer(0)]]) {
  float c[2][2][2] = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
  float3 v[2] = {};
  setv(v, 1);
  const float cc[3] = {4, 5, 6};
  o[0] = sum3(c);
  o[1] = v[1].y;
  o[2] = rd(cc, 2);
}
