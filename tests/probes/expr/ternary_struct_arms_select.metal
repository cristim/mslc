// EXPECT: valid
//
// Two struct locals are values like any other, so a conditional picks one.
struct S { float a; int b; };
kernel void ternary_struct_arms_select(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    S x;
    x.a = in[0];
    x.b = 1;
    S y;
    y.a = in[1];
    y.b = 2;
    S r = in[2] > 0.0f ? x : y;
    out[i] = r.a + float(r.b);
}
