// EXPECT: error which is not valid for [[threads_per_threadgroup]]
// Apple: "type 'packed_uint3' (packed vector of 3 'unsigned int' values) is not valid for attribute".
kernel void threads_per_threadgroup_packed_rejected(device float* o [[buffer(0)]],
                                                    packed_uint3 t [[threads_per_threadgroup]])
{
    o[0] = float(t.x);
}
