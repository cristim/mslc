// EXPECT: valid
// DISASM: FMin
// DISASM: SMin
// DISASM: UMin
// DISASM-NOT: FMax
// DISASM-NOT: SMax
// DISASM-NOT: UMax
kernel void min_by_type(device const float* f [[buffer(0)]],
                        device const int* s [[buffer(1)]],
                        device const uint* u [[buffer(2)]],
                        device float* fout [[buffer(3)]],
                        device int* sout [[buffer(4)]],
                        device uint* uout [[buffer(5)]],
                        uint i [[thread_position_in_grid]])
{
    fout[i] = min(f[i], f[i + 1u]);
    sout[i] = min(s[i], s[i + 1u]);
    uout[i] = min(u[i], u[i + 1u]);
}
