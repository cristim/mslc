// EXPECT: valid
// [[threads_per_threadgroup]] is the workgroup size, which the module already
// holds as three spec constants (SpecId 0, 1, 2) for indium to fill in. Apple
// takes uint, uint2, uint3, ushort, ushort2 and ushort3, and a vector narrower
// than the grid is the leading components. A ushort is a conversion of the
// constants.
// DISASM-MATCH: OpCompositeConstruct %v3ushort
// DISASM-MATCH: OpUConvert %ushort
kernel void threads_per_threadgroup_forms(device float* o [[buffer(0)]],
                                          ushort3 t [[threads_per_threadgroup]],
                                          ushort3 p [[thread_position_in_threadgroup]])
{
    if (p.x == 0 && p.y == 0 && p.z == 0) {
        o[0] = float(t.x);
        o[1] = float(t.y);
        o[2] = float(t.z);
    }
}
