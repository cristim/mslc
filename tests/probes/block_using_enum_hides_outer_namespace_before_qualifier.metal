// EXPECT: valid
// DISASM: OpReturnValue %int_1002
// DISASM-NOT: OpReturnValue %int_1001
//
// Apple: "using N::E;" brings the enum into the block, where it hides the outer namespace E before "::".
#include <metal_stdlib>
using namespace metal;
namespace E { constant int v = 1001; }
namespace N { enum E { v = 1002 }; }
int g(int x) { using N::E; return E::v; }
kernel void block_using_enum_hides_outer_namespace_before_qualifier(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = g(int(i)); }
