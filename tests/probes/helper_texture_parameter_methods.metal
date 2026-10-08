// EXPECT: valid
// read, get_width and get_height on a helper's texture are lowered as on an entry point's.
// DISASM: OpImageFetch
// DISASM: OpImageQuerySizeLod
#include <metal_stdlib>
using namespace metal;
float4 fetch(texture2d<float> t, uint2 p) { return t.read(p) + float4(t.get_width(), t.get_height(), 0, 0); }
kernel void k(texture2d<float> t [[texture(0)]], device float4* out [[buffer(0)]]) {
  out[0] = fetch(t, uint2(1, 2));
}
