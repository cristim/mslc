// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_3
namespace ns { constant int N = 3; }
kernel void array_length_constant_using_declaration(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    using ns::N;
    float a[N];
    a[2] = 1.0;
    out[i] = a[2];
}
