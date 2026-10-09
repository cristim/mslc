// EXPECT: valid
// A constant struct reference used whole, as a value, is copied out of the buffer.
struct V { float4 p; float w; };
kernel void local_reference_struct_copied_by_value(device float4 *out [[buffer(0)]], constant V *verts [[buffer(1)]]) {
  constant V &v = verts[0];
  V c = v;
  out[0] = c.p + float4(c.w);
}
