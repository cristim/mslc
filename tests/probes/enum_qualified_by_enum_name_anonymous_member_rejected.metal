// EXPECT: error no member named "w" in the enum "E"
//
// E::w where w is an enumerator of an unrelated anonymous enum, not E: rejected
// with the honest "no member" message, not the typedef-of-an-anonymous-enum
// message (E is a named enum, not a typedef, so the anonymous-typedef branch
// must not fire here).
#include <metal_stdlib>
using namespace metal;
enum E { v = 3 };
enum { w = 4 };
kernel void enum_qualified_by_enum_name_anonymous_member_rejected(device int *o [[buffer(0)]])
{
    o[0] = E::w;
}
