// EXPECT: valid
// A buffer parameter makes binding 0 the address block, so the first texture is binding
// 1 (Iridium: air.cpp:554-557 increments before the texture loop). Both are in set 0
// for a kernel.
// REFLECT: { "kind": "Buffer", "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 0, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "t" }
#include <metal_stdlib>
using namespace metal;
kernel void k(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]])
{ out[0] = t.read(uint2(0)); }
