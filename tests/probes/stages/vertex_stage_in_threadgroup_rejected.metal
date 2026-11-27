// EXPECT: error parameter "in" takes [[stage_in]] by value, so it cannot be in the threadgroup address space
//
// Apple rejects an address space on a by-value stage input.
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 position [[position]]; };
vertex Out vertex_stage_in_threadgroup_rejected(threadgroup In in [[stage_in]])
{ Out o; o.position = in.p; return o; }
