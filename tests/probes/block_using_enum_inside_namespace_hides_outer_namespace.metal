// EXPECT: valid
// DISASM: OpReturnValue %int_1002
// DISASM-NOT: OpReturnValue %int_1001
//
// Apple: the same from a function in a namespace.
#include <metal_stdlib>
using namespace metal;
namespace E { constant int v = 1001; }
namespace N { enum E { v = 1002 }; }
namespace M { int g(int x) { using N::E; return E::v; } }
kernel void block_using_enum_inside_namespace_hides_outer_namespace(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = M::g(int(i)); }
