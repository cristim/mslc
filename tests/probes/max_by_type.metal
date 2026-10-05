// EXPECT: valid
// DISASM: FMax
// DISASM: SMax
// DISASM: UMax
// DISASM-MATCH: OpExtInst %float %[0-9]+ FMax %
// DISASM-MATCH: OpExtInst %int %[0-9]+ SMax %
// DISASM-MATCH: OpExtInst %uint %[0-9]+ UMax %
// DISASM-NOT: FMin
// DISASM-NOT: SMin
// DISASM-NOT: UMin
kernel void max_by_type(device const float* f [[buffer(0)]],
                        device const int* s [[buffer(1)]],
                        device const uint* u [[buffer(2)]],
                        device float* fout [[buffer(3)]],
                        device int* sout [[buffer(4)]],
                        device uint* uout [[buffer(5)]],
                        uint i [[thread_position_in_grid]])
{
    fout[i] = max(f[i], f[i + 1u]);
    sout[i] = max(s[i], s[i + 1u]);
    uout[i] = max(u[i], u[i + 1u]);
}
