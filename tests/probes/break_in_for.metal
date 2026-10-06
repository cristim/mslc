// EXPECT: valid
// DISASM-MATCH: OpLoopMerge %[A-Za-z0-9_]+ %[A-Za-z0-9_]+ None
kernel void break_in_for(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint sum = 0u;
    for (uint j = 0u; j < 100u; j = j + 1u) {
        if (j * j > 50u) {
            break;
        }
        sum = sum + j;
    }
    out[i] = sum;
}
