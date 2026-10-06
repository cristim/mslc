// EXPECT: error parameter "in" takes [[stage_in]] by value, so it cannot be in the threadgroup address space
//
// Apple rejects an address space on a by-value stage input.
struct In { float4 position [[position]]; };
fragment float4 fragment_stage_in_threadgroup_rejected(threadgroup In in [[stage_in]])
{ return in.position; }
