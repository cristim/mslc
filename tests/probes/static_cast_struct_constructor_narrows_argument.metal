// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM: = OpConvertFToS %int
struct S { float x; S(int v) : x(float(v) * 4.0f) {} };
kernel void static_cast_struct_constructor_narrows_argument(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    S s = static_cast<S>(in[i]);
    out[i] = s.x;
}
