// EXPECT: valid
// DISASM: OpConstant %int 9
//
// An enumerator folds into a file-scope constant.
#include <metal_stdlib>
using namespace metal;
enum { Base = 4, Next };
constant int kBase = Base;
constant int kNext = Next + kBase;
kernel void enum_in_file_scope_initializer(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = kNext; }
