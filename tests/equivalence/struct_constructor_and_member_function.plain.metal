struct B
{
    float x;
    float y;
};

B B_B(float a)
{
    B self;
    self.x = 2.0;
    self.y = a;
    {
        self.y = self.y + 1.0;
    }
    return self;
}

float B_len(B self) { return self.x + self.y * 10.0; }

float B_twice(float a) { return a * 2.0; }

kernel void k(device float *out [[buffer(0)]])
{
    B b = B_B(3.0);
    B c = B_B(4.0);
    out[0] = B_len(b) + B_len(c) + B_twice(5.0);
}
