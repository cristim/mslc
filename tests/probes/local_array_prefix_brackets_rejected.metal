// EXPECT: error brackets are not allowed here
//
// xcrun metal rejects "float[2] v;" ("brackets are not allowed here") and accepts
// "float v[2];". mslc used to accept the first form and declare the local as its element
// (816 bytes of valid SPIR-V that read values the source never wrote); it now lowers the
// second and refuses the first as Apple does.
kernel void local_array_prefix_brackets_rejected(device float *out [[buffer(0)]],
                                    uint index [[thread_position_in_grid]])
{
    float[2] v;
    out[index] = 1.0;
}
