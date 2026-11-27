// EXPECT: error needs a constant integer argument
//
// A const initialised from a run-time value is not a constant (Apple: "array size is not a
// constant expression").
kernel void array_length_named_constant_const_runtime_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    const int n = int(i) + 1;
    float a[n];
    a[0] = 1.0;
    out[i] = a[0];
}
