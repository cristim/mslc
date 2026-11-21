// EXPECT: valid
// thread on a vector local, in each initialiser form Apple accepts (xcrun metal -c, exit 0).
kernel void thread_local_vector(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  thread float3 a(0);
  thread float3 b = {1, 2, 3};
  thread float3 c = float3(4);
  thread float3 d{5, 6, 7};
  o[i] = a.x + b.y + c.z + d.x;
}
