// EXPECT: error is not a sampler option
// M names a user namespace, so M::filter::linear is not the library's sampler option (Apple rejects it).
#include <metal_stdlib>
namespace U { namespace filter { constant int linear = 1; } }
namespace M = U;
kernel void kern(metal::texture2d<float> t [[texture(0)]], device float* out [[buffer(0)]]) { constexpr metal::sampler s(M::filter::linear); out[0] = 1; }
