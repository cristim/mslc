// EXPECT: valid
// DISASM: OpFunctionCall
struct S { float x; explicit S(float v) : x(v + 3.0f) {} };
kernel void static_cast_explicit_struct_constructor(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    S s = static_cast<S>(in[i]);
    out[i] = s.x;
}
