// EXPECT: valid
// A scalar is the x extent, as Apple measured on the GPU.
kernel void threads_per_threadgroup_scalar(device float* o [[buffer(0)]],
                                           uint t [[threads_per_threadgroup]],
                                           uint p [[thread_position_in_threadgroup]])
{
    if (p == 0) {
        o[0] = float(t);
    }
}
