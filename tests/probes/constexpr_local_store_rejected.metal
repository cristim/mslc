// EXPECT: error cannot store through "c"
//
// constexpr makes a local const. Apple: "cannot assign to variable 'c' with
// const-qualified type 'const float'".
kernel void constexpr_local_store_rejected(device float *out [[buffer(0)]])
{
    constexpr float c = 1.0;
    c = 2.0;
    out[0] = c;
}
