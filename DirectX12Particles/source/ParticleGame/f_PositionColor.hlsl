struct v2f
{
    float4 Color : COLOR;
};

float4 main(v2f i) : SV_Target
{
    return i.Color;
}