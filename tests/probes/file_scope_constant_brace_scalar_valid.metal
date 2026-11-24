// EXPECT: valid
// DISASM: OpConstant %int 5
// DISASM: OpConstant %float 1
// DISASM: OpConstantFalse
// DISASM: OpConstant %int 0
//
// Apple takes a file-scope constant's brace list in place of "=", an empty list is zero, and an
// int may initialise a float. mslc folds the same values (#390).
#include <metal_stdlib>
using namespace metal;

constant int k{5};
constant float f{1};
constant bool b{};
constant int zero{};

kernel void file_scope_constant_brace_scalar_valid(device int *out [[buffer(0)]])
{
    out[0] = k;
    out[1] = int(f);
    out[2] = int(b);
    out[3] = zero;
}
