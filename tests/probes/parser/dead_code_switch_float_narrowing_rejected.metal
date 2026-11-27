// EXPECT: error cannot be narrowed to an integer
//
// A float narrowed to int after a return inside a switch case is rejected by Apple.
kernel void dead_code_switch_float_narrowing_rejected(device uint *o [[buffer(0)]])
{
    switch (o[0]) {
        case 0:
            return;
            float f = 1.5f;
            int x{f};
            o[0] = x;
    }
}
