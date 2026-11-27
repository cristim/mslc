// EXPECT: error constructing a S is not lowered yet
//
// Apple: "no matching conversion for C-style cast from 'float' to 'S'".
struct S { float a; };
kernel void cast_to_a_struct_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    S s = (S)in[0];
    out[i] = s.a;
}
