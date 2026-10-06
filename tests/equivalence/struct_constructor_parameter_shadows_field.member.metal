struct P
{
    float x;
    float y;
    float z;
    P(float x, float y) : x(x), y(x + y) { this->z = y; }
};

kernel void k(device float *out [[buffer(0)]])
{
    P p(3.0, 4.0);
    out[0] = p.x * 100.0 + p.y * 10.0 + p.z;
}
