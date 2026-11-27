// EXPECT: error constructing a S is not lowered yet
//
// Apple: "no matching conversion for static_cast from 'float' to 'S'".
struct S { float a; };
kernel void static_cast_to_struct_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    S s = static_cast<S>(in[0]);
    out[i] = s.a;
}
