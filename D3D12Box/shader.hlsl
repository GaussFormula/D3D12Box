cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerPass:register(b1)
{
    float4x4    gView;
    float4x4    gInvView;
    float4x4    gProj;
    float4x4    gInvProj;
    float4x4    gViewProj;
    float4x4    gInvViewProj;
    float3      gEyePosW;
    float       cbPerObjectPad1;
    float2      gRenderTargerSize;
    float2      gInvRenderTargetSize;
    float       gNearZ;
    float       gFarZ;
    float       gTotalTime;
    float       gDeltaTime;
};

struct VertexIn
{
    float3 PosL:POSITION;
    float4 Color:COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut result;

    // Transform to homogeneous clip spaces.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    result.PosH = mul(posW, gViewProj);
    result.Color = vin.Color;
    return result;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}