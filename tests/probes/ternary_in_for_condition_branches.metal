// EXPECT: valid
// DISASM: = OpPhi %float
kernel void ternary_in_for_condition_branches(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int n = 0;
    for (int j = 0; j < (in[1] > 0.0f ? in[i] : 3.0f); j++) {
        n++;
    }
    out[i] = float(n);
}
