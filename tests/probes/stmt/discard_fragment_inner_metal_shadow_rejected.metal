// EXPECT: error no member named "discard_fragment" in the namespace "N::metal"
#include <metal_stdlib>
namespace N {
namespace metal { }
fragment ::metal::float4 discard_fragment_inner_metal_shadow_rejected()
{
    metal::discard_fragment();
    return ::metal::float4(1.0);
}
}
