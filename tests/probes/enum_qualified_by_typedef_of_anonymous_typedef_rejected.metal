// EXPECT: error qualifies an enumerator by the typedef of an anonymous enum
//
// A second typedef level over an anonymous-enum typedef: "typedef enum{...}
// T; typedef T U; U::v". The owner tag resolves back to T (the
// anonymous-enum typedef), not to U, so the fix must test the resolved
// tag's own declared-kind rather than the immediate owner's; this gets the
// same honest "not supported" message as T::v, not a false "no member
// named" one.
#include <metal_stdlib>
using namespace metal;
typedef enum { u = 22, v = 33 } T;
typedef T U;
kernel void enum_qualified_by_typedef_of_anonymous_typedef_rejected(device int *o [[buffer(0)]])
{
    o[0] = U::v;
}
