// EXPECT: error returns void, so its return cannot carry a value
//
// Every entry point is a void SPIR-V function. A value returned from a void MSL
// function used to be emitted as OpReturnValue in it, which spirv-val rejects.
kernel void return_value_from_void_rejected(device float* out [[buffer(0)]])
{
    out[0] = 1.0;
    return 1.0;
}
