// EXPECT: valid
// OpCapability has to be in the capabilities section, which comes first in the
// logical layout. A capability emitted while a type was being declared landed
// in the graph-definitions section and spirv-val rejected the module, so half,
// long and ulong produced nothing loadable.
// DISASM: OpCapability Float16
kernel void half_scalar(device const half* in [[buffer(0)]],
                        device half* out [[buffer(1)]],
                        uint i [[thread_position_in_grid]])
{ out[i] = in[i]; }
