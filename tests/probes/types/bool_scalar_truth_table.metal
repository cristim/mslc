// EXPECT: valid
// DISASM: OpLogicalEqual
// DISASM: OpLogicalNotEqual
kernel void truth_table(device int4 *out [[buffer(0)]]) {
    out[0] = int4(int(false == false), int(false == true), int(true == false), int(true == true));
    out[1] = int4(int(false != false), int(false != true), int(true != false), int(true != true));
}
