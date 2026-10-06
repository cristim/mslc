struct Q
{
    float v;
    Q(float a) : v(a) { }
    static float twice(float a) { return a * 2.0; }
    float again() const { return twice(v); }
};

kernel void k(device float *out [[buffer(0)]])
{
    Q q(3.0);
    out[0] = q.again();
}
