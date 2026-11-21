// EXPECT: valid
// DISASM: OpReturnValue %int_1001
// DISASM-NOT: OpReturnValue %int_1002
//
// Apple: the using declaration is in a sibling block, which is closed by the use.
#include <metal_stdlib>
using namespace metal;
namespace E { constant int v = 1001; }
namespace N { enum E { v = 1002 }; }
int g(int x) { { using N::E; } return E::v; }
kernel void sibling_block_using_enum_does_not_hide_outer_namespace(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = g(int(i)); }
