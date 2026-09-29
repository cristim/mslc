// EXPECT: valid
// From SPIR-V 1.4 the entry point's interface list covers every global the entry
// point uses, not only Input and Output, and spirv-val enforces it. mslc emitted
// 1.3 and left descriptors out of the list on purpose, so the version and the
// list have to move together: 1.5 with the old list is rejected.
// DISASM: ; Version: 1.5
kernel void spirv_15_complete_interface(device const uint* in [[buffer(0)]],
                                        device uint* out [[buffer(1)]],
                                        uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
