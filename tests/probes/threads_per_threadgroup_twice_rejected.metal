// EXPECT: error a function takes it once
// Apple: "declaration with attribute 'threads_per_threadgroup' already specified".
kernel void threads_per_threadgroup_twice_rejected(device float* o [[buffer(0)]],
                                                   uint a [[threads_per_threadgroup]],
                                                   uint b [[threads_per_threadgroup]])
{
    o[0] = float(a + b);
}
