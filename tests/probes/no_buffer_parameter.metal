// EXPECT: valid
// A kernel with no buffer parameter has no address to walk, but the addressing
// model is declared whether or not there is one, so the capability that model
// requires has to be declared too. Raising it only alongside the binding-0
// block left this module naming a capability it never declared.
// DISASM: OpCapability PhysicalStorageBufferAddresses
// DISASM: OpMemoryModel PhysicalStorageBuffer64 GLSL450
// DISASM-NO-MATCH: OpTypePointer PhysicalStorageBuffer
// DISASM-NO-MATCH: Binding
kernel void no_buffer_parameter(uint i [[thread_position_in_grid]])
{
    uint j = i;
    if (j > 0u) { j = 1u; }
}
