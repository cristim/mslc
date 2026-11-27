// EXPECT: error mslc does not support more than one address space on a type, found "device" after another
struct In { float4 position [[position]]; };
fragment float4 two_different_address_spaces_rejected(thread In device in [[stage_in]])
{ return in.position; }
