struct P
{
    float x;
    float y;
    float z;
};

P P_P(float x, float y)
{
    P self;
    self.x = x;
    self.y = x + y;
    {
        self.z = y;
    }
    return self;
}

kernel void k(device float *out [[buffer(0)]])
{
    P p = P_P(3.0, 4.0);
    out[0] = p.x * 100.0 + p.y * 10.0 + p.z;
}
