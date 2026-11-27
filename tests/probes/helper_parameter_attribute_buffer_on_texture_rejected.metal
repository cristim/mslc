// EXPECT: error parameter "t" of helper function "f" has an attribute, [[buffer]], that its type
// QUIRK (helper parameter attributes): [[buffer(n)]] suits a pointer or a reference, not a texture, as on
// an entry point; Apple's compiler refuses it on a helper too.
#include <metal_stdlib>
using namespace metal;
float f(texture2d<float> t [[buffer(0)]]) { return t.read(uint2(0)).x; }
kernel void k(device float* out [[buffer(0)]], texture2d<float> t [[texture(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(t); }
