// EXPECT: valid
// DISASM: = OpPhi %float
//
// A conditional whose values branch splits the loop condition over several
// blocks, and OpLoopMerge has to end the loop header, so the header branches to
// a separate condition block first.
kernel void ternary_in_while_condition_branches(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int n = int(in[0]);
    while (n < (in[1] > 0.0f ? in[i] : 3.0f)) {
        n++;
    }
    out[i] = float(n);
}
