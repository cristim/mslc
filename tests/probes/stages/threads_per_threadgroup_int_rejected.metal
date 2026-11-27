// EXPECT: error which is not valid for [[threads_per_threadgroup]]
// Apple: "type 'int' is not valid for attribute 'threads_per_threadgroup'".
kernel void threads_per_threadgroup_int_rejected(device float* o [[buffer(0)]],
                                                 int t [[threads_per_threadgroup]])
{
    o[0] = float(t);
}
