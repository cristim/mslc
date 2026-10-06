// EXPECT: error a type has one address space, found "device" after another
struct In { float4 position [[position]]; };
fragment float4 two_different_address_spaces_rejected(thread In device in [[stage_in]])
{ return in.position; }
