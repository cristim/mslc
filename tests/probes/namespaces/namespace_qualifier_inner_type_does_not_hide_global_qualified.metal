// EXPECT: valid
// DISASM-MATCH: OpStore [^ ]+ %int_1001
//
// Apple: "::K::y" starts at the file scope, so the typedef K in N hides nothing.
#include <metal_stdlib>
using namespace metal;
namespace K { constant int y = 1001; }
namespace N { typedef int K; constant int z = ::K::y; }
kernel void namespace_qualifier_inner_type_does_not_hide_global_qualified(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = N::z; }
