// EXPECT: valid
// DISASM: OpEntryPoint Vertex
//
// Apple accepts a by-value stage input in the thread address space, volatile,
// and either of them either side of the type name.
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 position [[position]]; };
vertex Out stage_in_thread_and_volatile_qualifiers_accepted(const thread volatile In in [[stage_in]])
{ Out o; o.position = in.p; return o; }
