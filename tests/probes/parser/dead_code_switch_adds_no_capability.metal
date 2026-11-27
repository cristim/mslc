// EXPECT: valid
// DISASM-NOT: OpCapability Float16
// DISASM-NOT: OpCapability Int16
// DISASM-NOT: OpCapability Int8
//
// Declarations after a return in a switch case are type-checked but never reach the module.
kernel void dead_code_switch_adds_no_capability(device uint *o [[buffer(0)]])
{
    switch (o[0]) {
        case 0:
            return;
            half h = 1.0h;
            short s = 2;
            char c = 1;
            constexpr int n = 3;
            o[0] = uint(h) + s + c + n;
    }
}
