// EXPECT: valid
// DISASM-NOT: OpCapability Float16
// DISASM-NOT: OpCapability Int16
//
// Code after a return is type-checked but never reaches the module: it adds no capability,
// type or constant, so the device is not asked to support half or short for code that
// cannot run.
kernel void dead_code_adds_no_capability(device uint *o [[buffer(0)]])
{
    o[0] = 1;
    return;
    half h = 1.0h;
    short s = 2;
    o[0] = uint(h) + s;
}
