// EXPECT: valid
// DISASM: OpConstant %ulong 9223372036854775806
// DISASM: OpConstantTrue
//
// The sizeof of a name folds as a ulong in a file-scope constant: 4 - 8 wraps to 2^64 - 4, so
// it is greater than zero and halves to 9223372036854775806. Apple's compiler gives 1 and
// 9223372036854775806; a signed fold would give 0 and -2.
#include <metal_stdlib>
using namespace metal;
constant float g = 1.0f;
constant bool kPositive = sizeof(g) - 8 > 0;
constant ulong kHalved = (sizeof(g) - 8) / 2;
kernel void sizeof_expression_fold_is_unsigned(device ulong *out [[buffer(0)]])
{
    out[0] = kHalved;
    out[1] = kPositive;
}
