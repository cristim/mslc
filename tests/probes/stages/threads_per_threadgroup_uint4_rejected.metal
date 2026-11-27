// EXPECT: error which is not valid for [[threads_per_threadgroup]]
// Apple rejects a four-wide vector; the grid has three dimensions.
kernel void threads_per_threadgroup_uint4_rejected(device float* o [[buffer(0)]],
                                                   uint4 t [[threads_per_threadgroup]])
{
    o[0] = float(t.x);
}
