// EXPECT: error cannot be narrowed
//
// A constexpr narrowing after a return inside a switch case is rejected by Apple.
kernel void dead_code_switch_constexpr_narrowing_rejected(device uint *o [[buffer(0)]])
{
    switch (o[0]) {
        case 0:
            return;
            constexpr int n = 300;
            char x{n};
            o[0] = x;
    }
}
