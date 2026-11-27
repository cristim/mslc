// EXPECT: valid
//
// Apple accepts unsigned unsigned and short short with a warning (-Wduplicate-decl-specifier), so mslc accepts them.
kernel void k(device uint* o [[buffer(0)]])
{
    unsigned unsigned a = 1;
    short short b = 2;
    o[0] = a + (uint)b;
}
