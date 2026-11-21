// EXPECT: valid
// DISASM-MATCH: OpStore [^ ]+ %int_1002 
// DISASM-NO-MATCH: OpStore [^ ]+ %int_1001 
//
// Apple: E::v is the enumerator (1002). The enum E found in N hides the outer namespace E before the "::" ([basic.lookup.qual]/1).
#include <metal_stdlib>
using namespace metal;
namespace E { constant int v = 1001; }
namespace N { enum E { v = 1002 }; constant int z = E::v; }
kernel void namespace_qualifier_hidden_by_inner_enum(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = N::z; }
