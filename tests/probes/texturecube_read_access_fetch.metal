// EXPECT: valid
// DISASM-MATCH: OpTypeImage %float 2D 2 1 0 1 Unknown
// DISASM-NOT: Cube
// DISASM-MATCH: OpImageFetch %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod
// DISASM-MATCH: OpImageQuerySizeLod %v3uint
// DISASM-MATCH: OpUDiv %uint %[0-9A-Za-z_]+ %uint_6
// REFLECT: "texture_access": "Read"
//
// An access::read cube or cube array is the 2D array of its faces, as a fetch takes no cube image: read(coord, face
// [, layer], lod) reads layer * 6 + face at the level given, its size is that of the faces, and a cube array's
// array size is the number of faces over six.
#include <metal_stdlib>
using namespace metal;
kernel void k(texturecube<float, access::read> cube [[texture(0)]], texturecube_array<float, access::read> cubes [[texture(1)]],
              device float4* out [[buffer(0)]], uint2 p [[thread_position_in_grid]], constant uint& level [[buffer(1)]]) {
	out[0] = cube.read(p, 3u, level) + cubes.read(p, 2u, 1u, level);
	out[1] = float4(cube.get_width(level), cube.get_height(), cubes.get_width(), cubes.get_array_size());
}
