// EXPECT: error cannot be narrowed
//
// A narrowing inside an if body after a return is rejected by Apple.
kernel void dead_code_if_body_narrowing_rejected(device uint *o [[buffer(0)]], constant bool &t [[buffer(1)]])
{
    return;
    if (t) {
        uchar x{300};
        o[0] = x;
    }
}
