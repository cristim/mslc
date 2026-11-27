// EXPECT: valid
// DISASM-MATCH: OpLoad %v4float %[A-Za-z0-9_]+ Aligned 16
// The one reference the corpus needs: a const device struct bound to a buffer element.
struct V { float4 p; float w; };
kernel void local_reference_to_buffer_struct_element(device float4 *out [[buffer(0)]],
    constant V *verts [[buffer(1)]], uint vid [[thread_position_in_grid]]) {
  const device V &vert = verts[vid];
  out[vid] = vert.p * vert.w;
}
