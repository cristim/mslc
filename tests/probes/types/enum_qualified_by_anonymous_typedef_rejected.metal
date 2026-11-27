// EXPECT: error qualifies an enumerator by the typedef of an anonymous enum
//
// Apple accepts "T::v" for a typedef of an anonymous enum; mslc records an
// anonymous enum's enumerators with owner "<anonymous>", indistinguishable
// from another anonymous enum's, so this is a deliberate reject with an
// honest "not supported" message instead of the misleading "not a namespace".
#include <metal_stdlib>
using namespace metal;
typedef enum { u = 22, v = 33 } T;
kernel void enum_qualified_by_anonymous_typedef_rejected(device int *o [[buffer(0)]])
{
    o[0] = T::v;
}
