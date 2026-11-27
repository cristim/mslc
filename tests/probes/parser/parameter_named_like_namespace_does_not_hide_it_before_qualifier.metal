// EXPECT: valid
// DISASM: %int_1001
// DISASM-NOT: %int_1002
//
// Apple: a parameter named E does not hide namespace E before ::, only a type does.
#include <metal_stdlib>
using namespace metal;
namespace E { constant int v = 1001; }
namespace N { enum E { v = 1002 }; }
int g(int E) { return E::v + E; }
kernel void parameter_named_like_namespace_does_not_hide_it_before_qualifier(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = g(int(i)); }
