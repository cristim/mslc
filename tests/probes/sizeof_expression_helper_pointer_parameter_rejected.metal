// EXPECT: error sizeof of this expression is not lowered
// Apple: a helper's thread pointer parameter is a pointer, sizeof 8. The helper binds it as it
// binds a local array, so a local array of that name could not be told from it; an operand
// that names one is refused (an entry point's pointer parameter is told apart, see
// sizeof_expression_pointer_parameter_operands).
#include <metal_stdlib>
using namespace metal;
ulong size(thread float *p) { return sizeof((0, p)); }
kernel void sizeof_expression_helper_pointer_parameter_rejected(device ulong *out [[buffer(0)]])
{
    float x[2];
    out[0] = size(x);
}
