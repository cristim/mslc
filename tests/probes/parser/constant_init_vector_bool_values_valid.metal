// EXPECT: valid
// DISASM: OpConstantComposite %v4int %int_1 %int_2 %int_0 %int_4
// DISASM: OpConstantComposite %v4int %int_1_0 %int_1_1 %int_1_2 %int_1_3
// DISASM: OpConstantComposite %v4bool %true %false %true_0 %false_0
// DISASM: OpConstantComposite %v4float %float_1 %float_0 %float_1_0 %float_0_0
//
// Apple accepts a bool inside a vector constructor in a file-scope constant and converts it
// to 1 or 0: true is 1 in int4 and float4, and any nonzero bool4 element is true. mslc folds
// the same values, and diffcheck at seed 1 matches Apple on the stored result (#160).
#include <metal_stdlib>
using namespace metal;

constant int4 kA = int4(true, 2, false, 4);
constant int4 kB = int4(true);
constant bool4 kC = bool4(1, 0, 2, 0);
constant float4 kD = float4(true, false, 1, 0);

kernel void constant_init_vector_bool_values_valid(device float4 *out [[buffer(0)]])
{
    out[0] = float4(kA + kB + int4(kC) + int4(kD));
}
