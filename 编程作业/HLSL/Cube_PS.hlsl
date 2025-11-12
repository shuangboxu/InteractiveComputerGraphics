// Pixel shader implementing Blinn-Phong lighting for assignment 4
//
// ==== 许双博第四次作业修改 ====
// 新增光照计算，包括方向光、点光源、聚光灯。像素颜色由材质、灯光、法线以及
// 顶点颜色共同决定。

struct Light
{
    float3 position;   float range;
    float3 direction;  float spot;
    float3 ambient;    float pad0;
    float3 diffuse;    float pad1;
    float3 specular;   float pad2;
    int    type;
    int    enabled;
    int2   pad3;
};

struct Material
{
    float3 ambient;  float pad0;
    float3 diffuse;  float pad1;
    float3 specular; float shininess;
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

// 与顶点着色器对应的插值输出
struct VertexOut
{
    float4 posH    : SV_POSITION;
    float3 posW    : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float4 color   : COLOR0;
};

// 计算方向光贡献
float3 CalcDirLight(int idx, float3 normal, float3 viewDir)
{
    float3 L = normalize(-lights[idx].direction);
    float NdotL = saturate(dot(normal, L));
    float3 H = normalize(L + viewDir);
    float specFactor = 0.0f;
    if (NdotL > 0.0f)
    {
        specFactor = pow(saturate(dot(normal, H)), material.shininess);
    }
    float3 ambient  = lights[idx].ambient * material.ambient;
    float3 diffuse  = lights[idx].diffuse * material.diffuse * NdotL;
    float3 specular = lights[idx].specular * material.specular * specFactor;
    return ambient + diffuse + specular;
}

// 计算点光源贡献
//float3 CalcPointLight(int idx, float3 pos, float3 normal, float3 viewDir)
//{
//    float3 toLight = lights[idx].position - pos;
//    float dist = length(toLight);
//    if (dist >= lights[idx].range)
//        return float3(0.0, 0.0, 0.0);
//    float3 L = toLight / dist;
//    float NdotL = saturate(dot(normal, L));
//    float3 H = normalize(L + viewDir);
//    float specFactor = 0.0f;
//    if (NdotL > 0.0f)
//    {
//        specFactor = pow(saturate(dot(normal, H)), material.shininess);
//    }
//    // 简单线性衰减
//    float atten = saturate(1.0f - dist / lights[idx].range);
//    float3 ambient = lights[idx].ambient * material.ambient;
//    float3 diffuse = lights[idx].diffuse * material.diffuse * NdotL;
//    float3 specular = lights[idx].specular * material.specular * specFactor;
//    return (ambient + diffuse + specular) * atten;
//}
// ==== 许双博第四次作业修改（加强版） ====
// 点光源：更强中心亮度 + 快速衰减，模拟萤火虫式照明
float3 CalcPointLight(int idx, float3 pos, float3 normal, float3 viewDir)
{
    float3 toLight = lights[idx].position - pos;
    float dist = length(toLight);
    if (dist >= lights[idx].range)
        return float3(0.0, 0.0, 0.0);

    float3 L = toLight / dist;
    float NdotL = saturate(dot(normal, L));

    float3 H = normalize(L + viewDir);
    float specFactor = 0.0f;
    if (NdotL > 0.0f)
    {
        specFactor = pow(saturate(dot(normal, H)), material.shininess);
    }

    // ==== 改进衰减模型 ====
    // 原来：线性衰减 -> saturate(1 - dist/range)
    // 新：非线性衰减，让中心极亮、边缘快速熄灭
    float att = pow(saturate(1.0f - dist / lights[idx].range), 4.0f);

    // 如果希望中心更亮，可额外放大亮度（模拟强光闪烁）
    float intensityBoost = 2.5f; // 倍增亮度
    float3 ambient = lights[idx].ambient * material.ambient;
    float3 diffuse = lights[idx].diffuse * material.diffuse * NdotL;
    float3 specular = lights[idx].specular * material.specular * specFactor;

    return (ambient + (diffuse + specular) * intensityBoost) * att;
}

// 计算聚光灯贡献
float3 CalcSpotLight(int idx, float3 pos, float3 normal, float3 viewDir)
{
    float3 lightVec = pos - lights[idx].position;        // 光源指向像素
    float dist = length(lightVec);
    if (dist >= lights[idx].range)
        return float3(0.0, 0.0, 0.0);
    float3 dirToPixel = normalize(lightVec);
    float cosAlpha = dot(normalize(lights[idx].direction), dirToPixel);
    float cosCone = cos(lights[idx].spot);
    if (cosAlpha <= cosCone)
        return float3(0.0, 0.0, 0.0);
    // 平滑边缘：根据角度差计算因子
    float spotFactor = saturate((cosAlpha - cosCone) / (1.0f - cosCone));
    float3 L = -dirToPixel; // 从像素指向光源
    float NdotL = saturate(dot(normal, L));
    float3 H = normalize(L + viewDir);
    float specFactor = 0.0f;
    if (NdotL > 0.0f)
    {
        specFactor = pow(saturate(dot(normal, H)), material.shininess);
    }
    float attenRange = saturate(1.0f - dist / lights[idx].range);
    float atten = attenRange * spotFactor;
    float3 ambient  = lights[idx].ambient * material.ambient;
    float3 diffuse  = lights[idx].diffuse * material.diffuse * NdotL;
    float3 specular = lights[idx].specular * material.specular * specFactor;
    return (ambient + diffuse + specular) * atten;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float3 normal = normalize(pin.normalW);
    float3 viewDir = normalize(eyePos - pin.posW);
    float3 colorSum = float3(0.0, 0.0, 0.0);
    // 累加三个光源的贡献
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        if (lights[i].enabled == 0)
            continue;
        if (lights[i].type == 0)
        {
            colorSum += CalcDirLight(i, normal, viewDir);
        }
        else if (lights[i].type == 1)
        {
            colorSum += CalcPointLight(i, pin.posW, normal, viewDir);
        }
        else if (lights[i].type == 2)
        {
            colorSum += CalcSpotLight(i, pin.posW, normal, viewDir);
        }
    }
    // 将顶点颜色作为基色调制
    //colorSum *= pin.color.rgb;
    
    // 不再使用随机顶点颜色，而用材质的主色作为整体基色
    colorSum *= material.diffuse;
    
    // ==== 添加雾效 ====
    //float fogStart = 20.0f; // 雾开始的距离
    //float fogEnd = 45.0f; // 雾结束的距离
    //float3 fogColor = float3(0.05f, 0.08f, 0.12f); // 淡蓝灰雾

   // float dist = length(eyePos - pin.posW);
   // float fogFactor = saturate((fogEnd - dist) / (fogEnd - fogStart));

// 雾越远越浓，混合到最终颜色
   // colorSum = lerp(fogColor, colorSum, fogFactor);

    return float4(colorSum, 1.0f);
}