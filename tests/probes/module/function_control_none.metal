// EXPECT: valid
// A function that writes memory is not Pure, and none of the other function
// control hints are ever justified here.
// DISASM: OpFunction %void None
kernel void function_control_none(device uint* out [[buffer(0)]],
                                  uint i [[thread_position_in_grid]])
{ out[i] = i; }
