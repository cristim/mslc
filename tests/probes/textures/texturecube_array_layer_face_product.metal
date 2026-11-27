// EXPECT: valid
// A cube array storage coordinate is (x, y, 6 * array + face): the array index is multiplied, the face added.
// DISASM-MATCH: OpIMul %uint %uint_5 %uint_6
// DISASM-MATCH: OpIAdd %uint %[0-9A-Za-z_]+ %uint_2
#include <metal_stdlib>
using namespace metal;
kernel void hit(texturecube_array<float, access::write> a [[texture(0)]]) {
  a.write(float4(1), uint2(1), 2u, 5u);
}
