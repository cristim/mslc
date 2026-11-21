// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-MATCH: OpConstant %ulong 16[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
//
// A texture is a handle of 8 bytes whatever its kind, access or component type, and an array
// of two is 16. Apple: 8 for texture2d<float>, depth2d<float>, texturecube<half>, a write-only
// texture2d<int> and a texture3d, and 16 for the array.
#include <metal_stdlib>
using namespace metal;

kernel void sizeof_texture(device ulong *out [[buffer(0)]])
{
    out[0] = sizeof(texture2d<float>);
    out[1] = sizeof(depth2d<float>);
    out[2] = sizeof(texturecube<half>);
    out[3] = sizeof(texture2d<int, access::write>);
    out[4] = sizeof(texture3d<float>);
    out[5] = sizeof(texture2d<float>[2]);
}
