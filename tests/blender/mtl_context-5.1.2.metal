using namespace metal;
struct Vertex {
  float4 position [[position]];
  float2 texCoord [[attribute(0)]];
};

vertex Vertex vertex_shader(uint v_id [[vertex_id]]) {
  Vertex vtx;
  vtx.position.x = float(v_id & 1) * 4.0 - 1.0;
  vtx.position.y = float(v_id >> 1) * 4.0 - 1.0;
  vtx.position.z = 0.0;
  vtx.position.w = 1.0;
  vtx.texCoord = vtx.position.xy * 0.5 + 0.5;
  return vtx;
}

constexpr sampler s {};

fragment float4 fragment_shader(Vertex v [[stage_in]],
                texture2d<float> t [[texture(0)]]) {
  /* Final blit should ensure alpha is 1.0. This resolves
   * rendering artifacts for blitting of final back-buffer. */
  float4 out_tex = t.sample(s, v.texCoord);
  out_tex.a = 1.0;
  return out_tex;
}
