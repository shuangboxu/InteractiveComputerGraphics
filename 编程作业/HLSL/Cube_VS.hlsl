// Vertex shader implementing lighting for assignment 4
//
// ==== 许双博第四次作业修改 ====
// 此文件重写了原有顶点着色器，不再包含 Cube.hlsli，
// 新增了光源与材质结构，并将顶点位置与法线变换到世界空间。

struct Light
{
    float3 position;   float range;      // 位置和作用范围（点光/聚光）
    float3 direction;  float spot;       // 方向和聚光角度（弧度）
    float3 ambient;    float pad0;       // 环境光颜色和填充
    float3 diffuse;    float pad1;       // 漫反射颜色和填充
    float3 specular;   float pad2;       // 镜面反射颜色和填充
    int    type;       // 0=方向光 1=点光源 2=聚光灯
    int    enabled;    // 是否启用
    int2   pad3;       // 对齐填充
};

struct Material
{
    float3 ambient;  float pad0;      // 环境反射系数
    float3 diffuse;  float pad1;      // 漫反射系数
    float3 specular; float shininess; // 镜面反射系数与高光指数
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 proj;
    Light    lights[3];
    Material material;
    float3   eyePos;
    float    padEye;
};

// 顶点输入：位置、法线、颜色
struct VertexIn
{
    float3 posL    : POSITION;
    float3 normalL : NORMAL;
    float4 color   : COLOR;
};

// 顶点输出：裁剪空间位置、世界空间位置、世界空间法线、颜色
struct VertexOut
{
    float4 posH    : SV_POSITION; // 裁剪空间位置
    float3 posW    : TEXCOORD0;   // 世界空间位置
    float3 normalW : TEXCOORD1;   // 世界空间法线
    float4 color   : COLOR0;      // 颜色
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    // 变换到世界空间
    float4 posW4 = mul(float4(vin.posL, 1.0f), world);
    vout.posW = posW4.xyz;
    // 变换到裁剪空间
    float4 posV = mul(posW4, view);
    vout.posH = mul(posV, proj);
    // 法线只参与旋转缩放，不受平移影响
    vout.normalW = mul(vin.normalL, (float3x3)world);
    // 保留顶点颜色
    vout.color = vin.color;
    return vout;
}