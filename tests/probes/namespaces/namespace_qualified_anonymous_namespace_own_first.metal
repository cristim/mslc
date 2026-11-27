// EXPECT: valid
// DISASM-MATCH: OpReturnValue %int_11[_0-9]*
// DISASM-NO-MATCH: %int_22
//
// A::f() calls A's own f, not the f declared in A's nested anonymous
// namespace (an implicit using directive of A).
#include <metal_stdlib>
using namespace metal;
namespace A {
  namespace { int f() { return 22; } }
  int f() { return 11; }
}
kernel void namespace_qualified_anonymous_namespace_own_first(device int *o [[buffer(0)]])
{
    o[0] = A::f();
}
