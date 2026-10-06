// EXPECT: error texture2d<uint> is not lowered
//
// The sampled type is read whole, so the diagnostic names the type read whole (uint), not the first word.
kernel void k(texture2d<unsigned int> t [[texture(0)]], device uint* o [[buffer(0)]])
{
    o[0] = t.get_width();
}
