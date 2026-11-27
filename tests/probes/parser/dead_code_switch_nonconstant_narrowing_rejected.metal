// EXPECT: error requires a supported constant expression
//
// A non-constant narrowing after a return inside a switch case is rejected by Apple.
kernel void dead_code_switch_nonconstant_narrowing_rejected(device uint *o [[buffer(0)]])
{
    switch (o[0]) {
        case 0:
            return;
            int n = 300;
            char x{n};
            o[0] = x;
    }
}
