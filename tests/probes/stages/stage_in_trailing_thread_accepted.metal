// EXPECT: valid
// DISASM: OpEntryPoint Fragment
struct In { float4 position [[position]]; };
fragment float4 stage_in_trailing_thread_accepted(In thread const in [[stage_in]])
{ return in.position; }
