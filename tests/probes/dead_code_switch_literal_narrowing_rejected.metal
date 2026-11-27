// EXPECT: error cannot be narrowed
//
// A narrowing after a return inside a switch case is rejected by Apple.
kernel void dead_code_switch_literal_narrowing_rejected(device uint *o [[buffer(0)]])
{
    switch (o[0]) {
        case 0:
            return;
            uchar x{300};
            o[0] = x;
    }
}
