// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// Textures differ by access and by component type: s(a) reaches the overload of a float
// texture sampled, not the half one or the write one.
#include <metal_stdlib>
using namespace metal;
float s(texture2d<float> t) { return 1.5f; }
float s(texture2d<half> t) { return 7.5f; }
float s(texture2d<float, access::write> t) { return 7.5f; }
kernel void k(texture2d<float> a [[texture(0)]], device float* out [[buffer(0)]]) { out[0] = s(a); }
