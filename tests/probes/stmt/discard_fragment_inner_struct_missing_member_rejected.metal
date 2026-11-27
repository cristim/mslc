// EXPECT: error function "N::metal::discard_fragment" is not a builtin mslc recognises
#include <metal_stdlib>
namespace N {
struct metal { };
fragment ::metal::float4 discard_fragment_inner_struct_missing_member_rejected()
{
    metal::discard_fragment();
    return ::metal::float4(1.0);
}
}
