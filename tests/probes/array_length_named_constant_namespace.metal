// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_6
namespace cfg { constant int N = 2; constant int M = N * 3; }
using cfg::N;

kernel void array_length_named_constant_namespace(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[cfg::M];
    float b[N];
    a[5] = 1.0; b[1] = 2.0;
    out[i] = a[5] + b[1];
}
