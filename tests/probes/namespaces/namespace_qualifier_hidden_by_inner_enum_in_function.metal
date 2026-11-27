// EXPECT: valid
// DISASM: OpReturnValue %int_1002
// DISASM-NOT: OpReturnValue %int_1001
//
// Apple: the same hiding from a function two namespaces down, with the enum in the middle one.
#include <metal_stdlib>
using namespace metal;
namespace E { constant int v = 1001; }
namespace N { enum E { v = 1002 }; namespace M { int f(int a) { return (E::v); } } }
kernel void namespace_qualifier_hidden_by_inner_enum_in_function(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = N::M::f(0); }
