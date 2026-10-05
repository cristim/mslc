// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %bool %[_0-9a-zA-Z]+ %float_0[^_0-9a-zA-Z]
// DISASM-MATCH: OpLoopMerge
//
// A while condition is compared with zero like an if condition, and the compare
// sits in the loop header, ahead of OpLoopMerge and the branch.
kernel void while_condition_from_float(device float *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    float x = out[i];
    while (x) {
        x = x - 1.0;
    }
    out[i] = x;
}
