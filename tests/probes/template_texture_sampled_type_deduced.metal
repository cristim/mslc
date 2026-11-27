// EXPECT: valid
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
// DISASM: OpTypeImage %int 2D 2 0 0 1 Unknown
// DISASM: OpTypeImage %uint 2D 0 0 0 2 Unknown
// texture2d<T> takes T from the argument texture, as a parameter of type T does from a value (C++
// [temp.deduct.call]): each call reads through an instance for its texture's sampled type, as the same
// functions written for float, int and uint do.
#include <metal_stdlib>
using namespace metal;
template <typename T> T rd(texture2d<T> t, uint2 p) { return t.read(p).x; }
template <typename T> T first(texture2d<T, access::read> t, T scale) { return t.read(uint2(0)).x * scale; }
template <typename T> float both(texture2d<T> a, texture2d<T> b) { return 1.0f; }
fragment float4 f(texture2d<float> tf [[texture(0)]], texture2d<int> ti [[texture(1)]], texture2d<uint, access::read> tu [[texture(2)]], float4 pos [[position]]) {
  float a = rd(tf, uint2(pos.xy));
  int b = rd(ti, uint2(pos.xy));
  uint c = first(tu, 3u);
  return a + float4(b) + float(c) + both(tf, tf);
}
