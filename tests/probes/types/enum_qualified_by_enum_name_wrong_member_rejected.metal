// EXPECT: error no member named "w" in the enum "A::E"
//
// A::E::w where w is an enumerator of a different enum F, not A::E: rejected
// with the honest "no member" message rather than the old "is not a
// namespace" fallback.
#include <metal_stdlib>
using namespace metal;
namespace A { enum E { u = 22, v = 33 }; }
enum F { w = 44 };
kernel void enum_qualified_by_enum_name_wrong_member_rejected(device int *o [[buffer(0)]])
{
    o[0] = A::E::w;
}
