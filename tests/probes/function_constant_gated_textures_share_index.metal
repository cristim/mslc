// EXPECT: valid
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 0, "binding": 1 }, "texture_access": "Sample", "param_index": 1, "name": "flat" }
// REFLECT: { "kind": "Texture", "metal_index": 0, "descriptor": { "set": 0, "binding": 2 }, "texture_access": "Read", "param_index": 2, "name": "layered" }
//
// A source taken as a texture2d or a texture2d_array at texture(0), a function constant picking one: each texture has
// a binding and a type of its own, and only the one the constant selects is read. One of the gated parameters
// may also be ungated, as Apple's compiler accepts (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
constant bool isArray [[function_constant(0)]];
kernel void k(device float* out [[buffer(0)]],
              texture2d<float> flat [[texture(0), function_constant(isArray)]],
              texture2d_array<float, access::read> layered [[texture(0)]]) {
	out[0] = isArray ? layered.read(uint2(0), 0).x : flat.read(uint2(0)).x;
}
