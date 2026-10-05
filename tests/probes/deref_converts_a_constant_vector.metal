// EXPECT: valid
// DISASM-MATCH: OpLoad %v2uint %[0-9]+ Aligned 8
// DISASM-MATCH: OpConvertUToF %v2float %[0-9]+
//
// The corpus spelling: a "constant uint2 *" buffer read whole and converted.
kernel void deref_converts_a_constant_vector(device float2* out [[buffer(0)]],
                                             constant uint2* size [[buffer(1)]])
{
    out[0] = float2(*size);
}
