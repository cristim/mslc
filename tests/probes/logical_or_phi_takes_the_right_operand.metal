// EXPECT: valid
// DISASM-MATCH: = OpPhi %bool %true[_0-9]* %[_0-9a-zA-Z]+ %false[_0-9]* %[_0-9a-zA-Z]+
//
// As for &&: the second value of the OpPhi is the constant right operand.
kernel void logical_or_phi_takes_the_right_operand(device uint *out [[buffer(0)]], constant int *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool a = v[i] > 0;
    if (a || false) {
        out[i] = 1u;
    }
}
