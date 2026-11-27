// EXPECT: error the array index of read has to be an integer
// The public MSL signature takes the layer as uint; a float layer is an implicit
// C++ conversion the spec does not list, so mslc rejects it loudly rather than guess
// the rounding. Not a claim that the source is invalid MSL.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]]) {
  return t.read(uint2(0, 0), 1.5);
}
