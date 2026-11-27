// EXPECT: error parameter "in" takes [[stage_in]] by value, so it cannot be in the device address space
//
// Apple rejects an address space on a by-value stage input.
struct In { float4 position [[position]]; };
fragment float4 fragment_stage_in_device_rejected(device In in [[stage_in]])
{ return in.position; }
