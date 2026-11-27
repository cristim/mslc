// EXPECT: valid
// DISASM: OpConstant %int 5
// DISASM: OpConstant %float 1
// DISASM: OpConstantFalse
// DISASM: OpConstant %int 0
// DISASM: OpConstant %int 7
//
// Apple takes a file-scope constant's brace list in place of "=", an empty list is zero, and an
// int may initialise a float. mslc folds the same values (#390).
#include <metal_stdlib>
using namespace metal;

enum Mode { MA, MB = 3 };

constant int k{5};
constant float f{1};
constant bool b{};
constant int zero{};
constant int eq_zero = {};
constant int eq_val = {7};
constant Mode m_val{MB};
constant Mode m_zero{};
constant int arr[zero + eq_zero + 1]{k};

kernel void file_scope_constant_brace_scalar_valid(device int *out [[buffer(0)]])
{
    char narrowed{zero};
    out[0] = k + arr[0] + int(narrowed);
    out[1] = int(f) + eq_val;
    out[2] = int(b) + int(m_val) + int(m_zero);
    out[3] = zero + eq_zero;
}
