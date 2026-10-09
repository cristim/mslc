// EXPECT: error field "pos" of "In" is [[position]], which collides
//
// The same collision as fragment_position_collides_with_stage_in_rejected.metal,
// but with the direct [[position]] parameter declared before the [[stage_in]]
// struct, so the struct's field is the one that reports the collision instead
// of the direct parameter. Parameters are processed in declaration order, so
// this pins that the guard catches the collision regardless of which source
// is seen first.
struct In { float4 pos [[position]]; };

fragment float4 fragment_position_collides_with_stage_in_reverse_order_rejected(float4 p [[position]],
    In in [[stage_in]])
{
    return in.pos * p;
}
