// EXPECT: error qualifies an enumerator by the typedef of an anonymous enum
//
// The cross-namespace form of the second-level anonymous-enum typedef:
// "namespace A{typedef enum{...} T;} namespace B{typedef A::T U;} B::U::v"
// gets the same honest "not supported" message as U::v, not a false
// "no member named" one.
#include <metal_stdlib>
using namespace metal;
namespace A { typedef enum { u = 22, v = 33 } T; }
namespace B { typedef A::T U; }
kernel void enum_qualified_by_typedef_of_anonymous_typedef_cross_namespace_rejected(device int *o [[buffer(0)]])
{
    o[0] = B::U::v;
}
