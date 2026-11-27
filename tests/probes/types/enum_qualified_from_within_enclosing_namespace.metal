// EXPECT: valid
// DISASM-MATCH: OpReturnValue %int_33[_0-9]*
//
// E::v written from inside namespace A, which declares E: the owner is
// resolved through unqualified lookup from inside A, a different route from
// the fully-qualified A::E::v forms.
#include <metal_stdlib>
using namespace metal;
namespace A {
  enum E { u = 22, v = 33 };
  int get() { return E::v; }
}
kernel void enum_qualified_from_within_enclosing_namespace(device int *o [[buffer(0)]])
{
    o[0] = A::get();
}
