// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %v2uint
// The whole vector is a value, not the leading components of a uvec3.
kernel void threads_per_threadgroup_uint2(device float* o [[buffer(0)]],
                                          uint2 t [[threads_per_threadgroup]],
                                          uint2 p [[thread_position_in_threadgroup]])
{
    uint2 whole = t;
    if (p.x == 0 && p.y == 0) {
        o[0] = float(whole.x * whole.y);
    }
}
