// EXPECT: valid
// DISASM-NOT: OpCapability Float16
//
// An uninitialized declaration after a return in a case that a later case does not name adds nothing to the module.
kernel void dead_code_switch_nonlast_adds_no_capability(device uint *o [[buffer(0)]])
{
    switch (o[0]) {
        case 0:
            return;
            half h;
        case 1:
            o[0] = 1;
    }
}
