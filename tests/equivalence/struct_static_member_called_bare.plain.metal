struct Q
{
    float v;
};

Q Q_Q(float a)
{
    Q self;
    self.v = a;
    return self;
}

float Q_twice(float a) { return a * 2.0; }

float Q_again(Q self) { return Q_twice(self.v); }

kernel void k(device float *out [[buffer(0)]])
{
    Q q = Q_Q(3.0);
    out[0] = Q_again(q);
}
