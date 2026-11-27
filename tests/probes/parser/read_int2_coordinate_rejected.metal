// EXPECT: error the coordinate of read has to be a uint2 or a ushort2
// Apple: no matching member function for call to 'read'.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read(int2(1, 2));
}
