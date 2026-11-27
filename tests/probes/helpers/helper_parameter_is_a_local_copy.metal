// EXPECT: valid
// The parameter is stored through, so it is a variable of the helper's own
// and not the caller's.
// DISASM-MATCH: OpFunctionParameter %float
// DISASM-MATCH: OpVariable %_ptr_Function_float Function
#include <metal_stdlib>
using namespace metal;
float clobber(float x) { x = x + 10.0f; return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    float a = 1.0f;
    float b = clobber(a);
    out[0] = a;
    out[1] = b;
}
