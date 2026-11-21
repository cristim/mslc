// EXPECT: valid
// DISASM-MATCH: %int_3 = OpConstant %int 3
//
// Apple folds a file-scope constant int into an enumerator, and "B" is then kBase + 1.
#include <metal_stdlib>
using namespace metal;
constant int kBase = 2;
enum One { A = kBase, B };
kernel void enum_value_named_constant(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = B; }
