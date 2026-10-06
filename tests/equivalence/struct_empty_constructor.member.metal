struct Item
{
    Item() {}
    float2 a;
    float4 b;
};
kernel void k(device float *out [[buffer(0)]], device const Item *items [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    Item local;
    local.a = items[i].a;
    out[i] = local.a.x + items[i].b.w;
}
