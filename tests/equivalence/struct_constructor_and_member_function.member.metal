struct B
{
    float x;
    float y;
    B(float a) : y(a), x(2.0) { y = y + 1.0; }
    float len() const { return x + y * 10.0; }
    static float twice(float a) { return a * 2.0; }
};

kernel void k(device float *out [[buffer(0)]])
{
    B b(3.0);
    B c = B(4.0);
    out[0] = b.len() + c.len() + B::twice(5.0);
}
