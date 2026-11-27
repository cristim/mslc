// EXPECT: valid
// DISASM-MATCH: = OpPhi %bool %false[_0-9]* %[_0-9a-zA-Z]+ %true[_0-9]* %[_0-9a-zA-Z]+
//
// With a constant right operand the OpPhi's second value is that constant, so
// the match shows the join takes the right operand's value and not the left
// condition.
kernel void logical_and_phi_takes_the_right_operand(device uint *out [[buffer(0)]], constant int *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool a = v[i] > 0;
    if (a && true) {
        out[i] = 1u;
    }
}
