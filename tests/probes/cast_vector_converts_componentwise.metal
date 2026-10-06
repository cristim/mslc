// EXPECT: valid
// DISASM: = OpConvertSToF %v3float
// DISASM: = OpConvertFToS %v3int
// DISASM-NOT: OpBitcast
//
// Apple converts a C-style vector cast value by value, as the functional cast
// does: (float3)int3(7,-8,9) is (7.0,-8.0,9.0), not a bit reinterpretation.
kernel void cast_vector_converts_componentwise(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int3 iv = int3(int(in[0]));
    float3 f = (float3)iv;
    int3 back = (int3)f;
    out[i] = f.x + float(back.y);
}
