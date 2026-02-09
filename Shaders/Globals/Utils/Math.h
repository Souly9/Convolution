
float smoothStep(float x, float edge0, float edge1)
{
    float t = (x - edge0) / (edge1 - edge0);

    if (x < edge0)
    {
        return 0.0;
    }
    if (x > edge1)
    {
        return 1.0;
    }

    return t * t * (3.0 - 2.0 * t);
}