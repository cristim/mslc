// EXPECT: valid
// A packed_float3 member is an array of floats in the buffer and a vector in the program.
struct S { packed_float3 p; float f; };
kernel void local_reference_struct_packed_member(device float *o [[buffer(0)]], device S *s [[buffer(1)]]) {
  device S &r = s[0];
  o[0] = r.p.y;
  r.p.y = 8.0;
}
