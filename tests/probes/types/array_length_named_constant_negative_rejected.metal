// EXPECT: error between 0 and 4294967295
//
// A negative constant is not a length (Apple: "declared as an array with a negative size").
constant int N = -1;
kernel void array_length_named_constant_negative_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    out[i] = a[0];
}
