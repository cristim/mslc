// EXPECT: valid
// DISASM: OpEntryPoint Fragment
//
// The same address space written twice is one address space, and a different one is reported.
struct In { float4 position [[position]]; };
fragment float4 repeated_same_address_space_accepted(thread In thread in [[stage_in]])
{ return in.position; }
