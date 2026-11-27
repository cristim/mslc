// EXPECT: valid
//
// A declaration without an initializer after a return in one case is still visible to the next case.
kernel void dead_code_switch_later_case_uses_name(device uint *o [[buffer(0)]])
{
    switch (o[0]) {
        case 0:
            return;
            int n;
        case 1:
            n = 3;
            o[0] = n;
    }
}
