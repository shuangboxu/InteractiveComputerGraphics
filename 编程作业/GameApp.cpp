#include "GameApp.h"
#include "d3dUtil.h"
#include "DXTrace.h"
#include "NameVertices.h"

#include <cmath>
#include <algorithm>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

using namespace DirectX;

// ==== 许双博第四次作业修改：顶点布局包含位置、法线、颜色 ====
const D3D11_INPUT_ELEMENT_DESC GameApp::VertexPosColor::inputLayout[3] = {
    // 位置：3 * 4字节 = 12字节
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    // 法线：紧随位置之后，3 * 4字节 = 12字节
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    // 颜色：紧随法线之后，4 * 4字节 = 16字节
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

GameApp::GameApp(HINSTANCE hInstance)
    : D3DApp(hInstance), m_CBuffer()
{
}

GameApp::~GameApp()
{
    // 释放4个模型 
    for (auto& m : m_Models) { delete m; m = nullptr; }
}

bool GameApp::Init()
{
    if (!D3DApp::Init())       return false;
    if (!InitEffect())         return false;
    if (!InitResource())       return false;
    return true;
}

void GameApp::OnResize()
{
    D3DApp::OnResize();
    UpdateProjectionMatrix();   // ==== 保持飞行相机的投影矩阵 ====
    ApplyViewMatrix();          // ==== 视角更新 ====
}

void GameApp::UpdateScene(float dt)
{
    if (m_KeyCooldown > 0.0f) m_KeyCooldown -= dt;
    //angle += 0.5f * dt;//控制旋转

#define CLAMP(v, lo, hi)  ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

    if (m_KeyCooldown <= 0.0f)
    {
        if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) { m_N = CLAMP(m_N + 1, 1, 200); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.20f; }
        else if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) { m_N = CLAMP(m_N - 1, 1, 200); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.20f; }
        else if (GetAsyncKeyState(VK_OEM_4) & 0x8000) { m_Spacing = std::max(1.0f, m_Spacing - 0.5f); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.10f; }
        else if (GetAsyncKeyState(VK_OEM_6) & 0x8000) { m_Spacing = std::min(20.0f, m_Spacing + 0.5f); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.10f; }
        else if (GetAsyncKeyState(VK_OEM_COMMA) & 0x8000) { m_OrbitMax = std::max(0, m_OrbitMax - 1); m_OrbitMin = std::min(m_OrbitMin, m_OrbitMax); m_KeyCooldown = 0.10f; }
        else if (GetAsyncKeyState(VK_OEM_PERIOD) & 0x8000) { m_OrbitMax = std::min(6, m_OrbitMax + 1); m_KeyCooldown = 0.10f; }
        // 在处理光照按键前读取 Shift 状态
        bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000);
        // 光照开关：按 1/2/3 切换点光源、聚光灯、方向光
        if ((GetAsyncKeyState('1') & 0x8000) && !shiftPressed) {
            m_PointLightEnabled = !m_PointLightEnabled;
            m_Lights[0].enabled = m_PointLightEnabled ? 1 : 0;
            m_KeyCooldown = 0.20f;
        }
        else if ((GetAsyncKeyState('2') & 0x8000) && !shiftPressed) {
            m_SpotLightEnabled = !m_SpotLightEnabled;
            m_Lights[1].enabled = m_SpotLightEnabled ? 1 : 0;
            m_KeyCooldown = 0.20f;
        }
        else if ((GetAsyncKeyState('3') & 0x8000) && !shiftPressed) {
            m_DirLightEnabled = !m_DirLightEnabled;
            m_Lights[2].enabled = m_DirLightEnabled ? 1 : 0;
            m_KeyCooldown = 0.20f;
        }
        //else if (GetAsyncKeyState('1') & 0x8000) { SetCameraMode(CameraMode::FirstPerson); m_KeyCooldown = 0.20f; }
        //else if (GetAsyncKeyState('2') & 0x8000) { SetCameraMode(CameraMode::ThirdPerson); m_KeyCooldown = 0.20f; }
        //else if (GetAsyncKeyState('3') & 0x8000) { SetCameraMode(CameraMode::FreeFlight); m_KeyCooldown = 0.20f; }
        //else if (GetAsyncKeyState('0') & 0x8000) { SetCameraMode(CameraMode::AutoFit); m_KeyCooldown = 0.20f; }
        // 检查是否同时按下 Shift 键切换视角
        if (shiftPressed)
        {
            if (GetAsyncKeyState('1') & 0x8000) { SetCameraMode(CameraMode::FirstPerson); m_KeyCooldown = 0.20f; }
            else if (GetAsyncKeyState('2') & 0x8000) { SetCameraMode(CameraMode::ThirdPerson); m_KeyCooldown = 0.20f; }
            else if (GetAsyncKeyState('3') & 0x8000) { SetCameraMode(CameraMode::FreeFlight); m_KeyCooldown = 0.20f; }
            else if (GetAsyncKeyState('0') & 0x8000) { SetCameraMode(CameraMode::AutoFit); m_KeyCooldown = 0.20f; }
        }

    }

    // ==== 许双博第四次作业修改：更新光源动画和方向 ====
    // 更新累计时间
    m_TotalTime += dt;
    // 动画路径：萤火虫在字符森林间来回运动
    {
        float radius = (m_N - 1) * m_Spacing * 0.6f;
        float yBase  = 2.0f + (m_N * 0.2f);
        float x = std::sinf(m_TotalTime * 0.7f) * radius;
        float z = std::cosf(m_TotalTime * 1.3f) * radius;
        float y = yBase + std::sinf(m_TotalTime * 2.0f) * (radius * 0.1f);
        m_Lights[0].position = DirectX::XMFLOAT3(x, y, z);
        // 点光源颜色可以缓慢变化以模拟萤火虫色彩
        float t = (std::sinf(m_TotalTime * 0.5f) + 1.0f) * 0.5f;
        m_Lights[0].diffuse  = DirectX::XMFLOAT3(0.8f + 0.2f * t, 0.8f * (1.0f - t), 1.0f);
        m_Lights[0].specular = m_Lights[0].diffuse;
    }
    // 更新聚光灯位置和方向绑定到相机
    {
        // 位置跟随相机眼睛
        m_Lights[1].position = m_CameraPos;
        // 方向指向相机前方
        DirectX::XMVECTOR fwd = GetForwardVector(m_CameraYaw, m_CameraPitch);
        DirectX::XMStoreFloat3(&m_Lights[1].direction, fwd);
    }

    static float acc = 0.0f; static int frames = 0;
    acc += dt; frames++;
    if (acc >= 0.5f)
    {
        float fps = frames / acc;
        wchar_t title[256];
        const wchar_t* modeName = L"自由飞行";
        switch (m_CameraMode)
        {
        case CameraMode::AutoFit: modeName = L"自动取景"; break;
        case CameraMode::FirstPerson: modeName = L"第一人称"; break;
        case CameraMode::ThirdPerson: modeName = L"第三人称"; break;
        case CameraMode::FreeFlight: default: modeName = L"自由飞行"; break;
        }

        swprintf(title, 256, L"字符立方体  |  模式:%s  |  N=%d (主字=%d)  |  spacing=%.1f  |  叶子max=%d  |  FPS=%.1f",
            modeName, m_N, m_N * m_N * m_N, m_Spacing, m_OrbitMax, fps);
        SetWindowTextW(m_hMainWnd, title);
        acc = 0.0f; frames = 0;
    }

    switch (m_CameraMode)
    {
    case CameraMode::AutoFit:
        UpdateCameraForCube();
        break;
    case CameraMode::FirstPerson:
        UpdateFirstPersonCamera(dt);
        break;
    case CameraMode::ThirdPerson:
        UpdateThirdPersonCamera(dt);
        break;
    case CameraMode::FreeFlight:
    default:
        UpdateFlightCamera(dt);   // ==== 许双博第三次作业修改：更新飞行相机 ====
        break;
    }
}

// 小哈希，稳定随机选择 
static inline int PickId(int x, int y, int z, int extra = 0)
{
    // 常用哈希技巧：互质大数混合，最后取低两位
    unsigned int h = 2166136261u;
    h = (h ^ (unsigned int)(x * 73856093)) * 16777619u;
    h = (h ^ (unsigned int)(y * 19349663)) * 16777619u;
    h = (h ^ (unsigned int)(z * 83492791)) * 16777619u;
    h = (h ^ (unsigned int)(extra * 2654435761u));
    return (int)(h & 3u); // 0..3
}

void GameApp::DrawScene()
{
    assert(m_pd3dImmediateContext);
    assert(m_pSwapChain);

    static float black[4] = { 0, 0, 0, 1 };
    m_pd3dImmediateContext->ClearRenderTargetView(m_pRenderTargetView.Get(), reinterpret_cast<const float*>(&black));
    m_pd3dImmediateContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    const float c = (m_N - 1) * 0.5f;
    XMMATRIX mRotate = XMMatrixRotationX(angle) * XMMatrixRotationY(angle * 0.7f);

    for (int ix = 0; ix < m_N; ++ix)
        for (int iy = 0; iy < m_N; ++iy)
            for (int iz = 0; iz < m_N; ++iz)
            {
                // 稳定随机选择一个主字 id 
                int id = PickId(ix, iy, iz);

                // 绑定该 id 的 VB/IB
                UINT stride = sizeof(VertexPosColor);
                UINT offset = 0;
                m_pd3dImmediateContext->IASetVertexBuffers(0, 1, m_pVertexBuffers[id].GetAddressOf(), &stride, &offset);
                m_pd3dImmediateContext->IASetIndexBuffer(m_pIndexBuffers[id].Get(), DXGI_FORMAT_R16_UINT, 0);

                // —— 不同尺寸：伪随机缩放 ——
                float h = fabsf(sinf(ix * 12.9898f + iy * 78.233f + iz * 37.719f) * 43758.5453f);
                h -= floorf(h);
                float scale = 0.35f + 0.35f * h;
                XMMATRIX mScale = XMMatrixScaling(scale, scale, scale);

                XMMATRIX mTranslate = XMMatrixTranslation(
                    (ix - c) * m_Spacing,
                    (iy - c) * m_Spacing,
                    (iz - c) * m_Spacing
                );

                // 主字：S * R * T
                m_CBuffer.world = XMMatrixTranspose(mScale * mRotate * mTranslate);
                // ==== 许双博第四次作业修改：更新材质、光源和观察者位置 ====
                m_CBuffer.material = m_ObjectMaterials[id];
                m_CBuffer.eyePos   = m_CameraPos;
                // 拷贝光源数组
                for (int li = 0; li < 3; ++li) m_CBuffer.lights[li] = m_Lights[li];

                D3D11_MAPPED_SUBRESOURCE mappedData{};
                HR(m_pd3dImmediateContext->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
                memcpy_s(mappedData.pData, sizeof(m_CBuffer), &m_CBuffer, sizeof(m_CBuffer));
                m_pd3dImmediateContext->Unmap(m_pConstantBuffer.Get(), 0);

                m_pd3dImmediateContext->DrawIndexed(m_IndexCounts[id], 0, 0);

                // —— 子字围绕主字公转（随机挑选字）——
                int nOrbiters = m_OrbitMin + ((ix * 7 + iy * 13 + iz * 17) % (std::max(1, m_OrbitMax - m_OrbitMin + 1)));

                for (int k = 0; k < nOrbiters; ++k)
                {
                    int childId = PickId(ix, iy, iz, k + 12345);    // ==== 许双博改的：子字随机 id ====

                    // 绑定该子字的 VB/IB
                    m_pd3dImmediateContext->IASetVertexBuffers(0, 1, m_pVertexBuffers[childId].GetAddressOf(), &stride, &offset);
                    m_pd3dImmediateContext->IASetIndexBuffer(m_pIndexBuffers[childId].Get(), DXGI_FORMAT_R16_UINT, 0);

                    float phase = (ix * 23 + iy * 29 + iz * 31 + k * 11) * 0.37f;
                    XMMATRIX mChildRot = XMMatrixRotationY(angle * 1.6f + phase)
                        * XMMatrixRotationX(angle * 0.3f + phase * 0.2f);
                    XMMATRIX mChildScale = XMMatrixScaling(0.25f, 0.25f, 0.25f);
                    XMMATRIX mChildOffset = XMMatrixTranslation(m_OrbitRadius, 0.0f, 0.0f);

                    XMMATRIX worldChild = mChildScale * mChildOffset * mChildRot * mScale * mTranslate;
                    m_CBuffer.world = XMMatrixTranspose(worldChild);
                    // ==== 许双博第四次作业修改：更新材质、光源和观察者位置 ====  (子字)
                    m_CBuffer.material = m_ObjectMaterials[childId];
                    m_CBuffer.eyePos   = m_CameraPos;
                    for (int li = 0; li < 3; ++li) m_CBuffer.lights[li] = m_Lights[li];

                    HR(m_pd3dImmediateContext->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
                    memcpy_s(mappedData.pData, sizeof(m_CBuffer), &m_CBuffer, sizeof(m_CBuffer));
                    m_pd3dImmediateContext->Unmap(m_pConstantBuffer.Get(), 0);

                    m_pd3dImmediateContext->DrawIndexed(m_IndexCounts[childId], 0, 0);
                }
            }



    if (m_CameraMode != CameraMode::FirstPerson && m_pPlayerVertexBuffer && m_pPlayerIndexBuffer && m_PlayerIndexCount > 0)
    {
        UINT stride = sizeof(VertexPosColor);
        UINT offset = 0;
        m_pd3dImmediateContext->IASetVertexBuffers(0, 1, m_pPlayerVertexBuffer.GetAddressOf(), &stride, &offset);
        m_pd3dImmediateContext->IASetIndexBuffer(m_pPlayerIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

        XMMATRIX playerScale = XMMatrixScaling(1.5f, 2.5f, 1.5f);
        XMMATRIX playerRotation = XMMatrixRotationY(m_PlayerYaw);
        XMMATRIX playerTranslation = XMMatrixTranslation(m_PlayerPos.x, m_PlayerPos.y + 1.25f, m_PlayerPos.z);
        m_CBuffer.world = XMMatrixTranspose(playerScale * playerRotation * playerTranslation);
        // ==== 许双博第四次作业修改：玩家使用默认材质并更新光照 ====
        m_CBuffer.material = m_ObjectMaterials[0];
        m_CBuffer.eyePos   = m_CameraPos;
        for (int li = 0; li < 3; ++li) m_CBuffer.lights[li] = m_Lights[li];

        D3D11_MAPPED_SUBRESOURCE mappedData{};
        HR(m_pd3dImmediateContext->Map(m_pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
        memcpy_s(mappedData.pData, sizeof(m_CBuffer), &m_CBuffer, sizeof(m_CBuffer));
        m_pd3dImmediateContext->Unmap(m_pConstantBuffer.Get(), 0);

        m_pd3dImmediateContext->DrawIndexed(m_PlayerIndexCount, 0, 0);
    }
    HR(m_pSwapChain->Present(0, 0));
}

bool GameApp::InitEffect()
{
    ComPtr<ID3DBlob> blob;

    HR(CreateShaderFromFile(L"HLSL\\Cube_VS.cso", L"HLSL\\Cube_VS.hlsl", "VS", "vs_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pVertexShader.GetAddressOf()));

    HR(m_pd3dDevice->CreateInputLayout(VertexPosColor::inputLayout, ARRAYSIZE(VertexPosColor::inputLayout),
        blob->GetBufferPointer(), blob->GetBufferSize(), m_pVertexLayout.GetAddressOf()));

    HR(CreateShaderFromFile(L"HLSL\\Cube_PS.cso", L"HLSL\\Cube_PS.hlsl", "PS", "ps_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pPixelShader.GetAddressOf()));

    return true;
}

bool GameApp::InitResource()
{

    for (int i = 0; i < 4; ++i)
    {
        m_Models[i] = new NameVertices(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, i);

        // 顶点缓冲
        D3D11_BUFFER_DESC vbd{};
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.ByteWidth = sizeof(VertexPosColor) * m_Models[i]->GetVerticesCount();
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initVB{};
        initVB.pSysMem = m_Models[i]->GetNameVertices();
        HR(m_pd3dDevice->CreateBuffer(&vbd, &initVB, m_pVertexBuffers[i].GetAddressOf()));

        // 索引缓冲
        D3D11_BUFFER_DESC ibd{};
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.ByteWidth = sizeof(WORD) * m_Models[i]->GetIndexCount();
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initIB{};
        initIB.pSysMem = m_Models[i]->GetNameIndices();
        HR(m_pd3dDevice->CreateBuffer(&ibd, &initIB, m_pIndexBuffers[i].GetAddressOf()));

        m_IndexCounts[i] = m_Models[i]->GetIndexCount();
    }

    // 玩家立方体网格
    // ==== 许双博第四次作业修改：将立方体按三角形拆分顶点，并计算法线 ====
    {
        // 原始立方体的顶点位置和颜色（不含法线）
        struct TmpVertex { DirectX::XMFLOAT3 pos; DirectX::XMFLOAT4 color; };
        TmpVertex origVerts[] =
        {
            { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(0.15f, 0.6f, 0.95f, 1.0f) },
            { XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT4(0.15f, 0.6f, 0.95f, 1.0f) },
            { XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT4(0.15f, 0.6f, 0.95f, 1.0f) },
            { XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT4(0.15f, 0.6f, 0.95f, 1.0f) },
            { XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT4(0.05f, 0.35f, 0.75f, 1.0f) },
            { XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT4(0.05f, 0.35f, 0.75f, 1.0f) },
            { XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT4(0.05f, 0.35f, 0.75f, 1.0f) },
            { XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT4(0.05f, 0.35f, 0.75f, 1.0f) }
        };
        WORD origIndices[] =
        {
            0, 1, 2, 0, 2, 3,        // -Z 面
            4, 6, 5, 4, 7, 6,        // +Z 面
            4, 5, 1, 4, 1, 0,        // -X 面
            3, 2, 6, 3, 6, 7,        // +X 面
            1, 5, 6, 1, 6, 2,        // +Y 面
            4, 0, 3, 4, 3, 7         // -Y 面
        };
        std::vector<VertexPosColor> expandedVerts;
        std::vector<WORD>           expandedIndices;
        expandedVerts.reserve(_countof(origIndices));
        expandedIndices.reserve(_countof(origIndices));
        for (size_t i = 0; i < _countof(origIndices); i += 3)
        {
            WORD ia = origIndices[i];
            WORD ib = origIndices[i + 1];
            WORD ic = origIndices[i + 2];
            DirectX::XMFLOAT3 p0 = origVerts[ia].pos;
            DirectX::XMFLOAT3 p1 = origVerts[ib].pos;
            DirectX::XMFLOAT3 p2 = origVerts[ic].pos;
            DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&p0);
            DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&p1);
            DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&p2);
            DirectX::XMVECTOR e1 = DirectX::XMVectorSubtract(v1, v0);
            DirectX::XMVECTOR e2 = DirectX::XMVectorSubtract(v2, v0);
            DirectX::XMVECTOR n  = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(e1, e2));
            DirectX::XMFLOAT3 normal;
            DirectX::XMStoreFloat3(&normal, n);
            // 创建三个新顶点
            VertexPosColor vtx0; vtx0.pos = p0; vtx0.normal = normal; vtx0.color = origVerts[ia].color;
            VertexPosColor vtx1; vtx1.pos = p1; vtx1.normal = normal; vtx1.color = origVerts[ib].color;
            VertexPosColor vtx2; vtx2.pos = p2; vtx2.normal = normal; vtx2.color = origVerts[ic].color;
            WORD baseIndex = static_cast<WORD>(expandedVerts.size());
            expandedVerts.push_back(vtx0);
            expandedVerts.push_back(vtx1);
            expandedVerts.push_back(vtx2);
            expandedIndices.push_back(baseIndex);
            expandedIndices.push_back(baseIndex + 1);
            expandedIndices.push_back(baseIndex + 2);
        }
        // 创建顶点缓冲
        D3D11_BUFFER_DESC playerVbd{};
        playerVbd.Usage = D3D11_USAGE_IMMUTABLE;
        playerVbd.ByteWidth = static_cast<UINT>(sizeof(VertexPosColor) * expandedVerts.size());
        playerVbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA playerVBData{};
        playerVBData.pSysMem = expandedVerts.data();
        HR(m_pd3dDevice->CreateBuffer(&playerVbd, &playerVBData, m_pPlayerVertexBuffer.ReleaseAndGetAddressOf()));
        // 创建索引缓冲
        D3D11_BUFFER_DESC playerIbd{};
        playerIbd.Usage = D3D11_USAGE_IMMUTABLE;
        playerIbd.ByteWidth = static_cast<UINT>(sizeof(WORD) * expandedIndices.size());
        playerIbd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA playerIBData{};
        playerIBData.pSysMem = expandedIndices.data();
        HR(m_pd3dDevice->CreateBuffer(&playerIbd, &playerIBData, m_pPlayerIndexBuffer.ReleaseAndGetAddressOf()));
        m_PlayerIndexCount = static_cast<UINT>(expandedIndices.size());
    }

    // 常量缓冲
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR(m_pd3dDevice->CreateBuffer(&cbd, nullptr, m_pConstantBuffer.GetAddressOf()));

    // 初始化 CBuffer，UpdateCameraForCube / UpdateFlightCamera 会覆盖
    m_CBuffer.world = XMMatrixIdentity();
    UpdateProjectionMatrix();      // ==== 许双博第三次作业修改：初始化透视矩阵 ====
    ApplyViewMatrix();             // ==== 视角初始化 ====

    // IA 设置：拓扑 & 输入布局（VB/IB 在 Draw 时切换）
    m_pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pd3dImmediateContext->IASetInputLayout(m_pVertexLayout.Get());

    // VS/PS 绑定
    m_pd3dImmediateContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);
    m_pd3dImmediateContext->VSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());
    m_pd3dImmediateContext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);
    // 把常量缓冲同时绑定到像素着色器（很重要！）
    m_pd3dImmediateContext->PSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());


    // 调试名
    D3D11SetDebugObjectName(m_pVertexLayout.Get(), "VertexPosColorLayout");
    D3D11SetDebugObjectName(m_pConstantBuffer.Get(), "ConstantBuffer");
    D3D11SetDebugObjectName(m_pVertexShader.Get(), "Cube_VS");
    D3D11SetDebugObjectName(m_pPixelShader.Get(), "Cube_PS");
    for (int i = 0; i < 4; ++i) {
        char n1[32]; sprintf_s(n1, "VB_%d", i);
        char n2[32]; sprintf_s(n2, "IB_%d", i);
        D3D11SetDebugObjectName(m_pVertexBuffers[i].Get(), n1);
        D3D11SetDebugObjectName(m_pIndexBuffers[i].Get(), n2);
    }
    D3D11SetDebugObjectName(m_pPlayerVertexBuffer.Get(), "Player_VB");
    D3D11SetDebugObjectName(m_pPlayerIndexBuffer.Get(), "Player_IB");

    // ==== 许双博第四次作业修改：初始化材质和光照 ====
    // 为四个汉字模型设置不同的材质颜色和高光参数
    {
        using DirectX::XMFLOAT3;
        // 材质0：偏红色
        m_ObjectMaterials[0].ambient  = XMFLOAT3(0.3f, 0.05f, 0.05f);
        m_ObjectMaterials[0].diffuse  = XMFLOAT3(0.7f, 0.2f, 0.2f);
        m_ObjectMaterials[0].specular = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_ObjectMaterials[0].shininess= 32.0f;
        // 材质1：偏绿色
        m_ObjectMaterials[1].ambient  = XMFLOAT3(0.05f, 0.3f, 0.05f);
        m_ObjectMaterials[1].diffuse  = XMFLOAT3(0.2f, 0.7f, 0.2f);
        m_ObjectMaterials[1].specular = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_ObjectMaterials[1].shininess= 32.0f;
        // 材质2：偏蓝色
        m_ObjectMaterials[2].ambient  = XMFLOAT3(0.05f, 0.05f, 0.3f);
        m_ObjectMaterials[2].diffuse  = XMFLOAT3(0.2f, 0.2f, 0.7f);
        m_ObjectMaterials[2].specular = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_ObjectMaterials[2].shininess= 32.0f;
        // 材质3：偏黄色
        m_ObjectMaterials[3].ambient  = XMFLOAT3(0.3f, 0.3f, 0.05f);
        m_ObjectMaterials[3].diffuse  = XMFLOAT3(0.7f, 0.7f, 0.2f);
        m_ObjectMaterials[3].specular = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_ObjectMaterials[3].shininess= 32.0f;
    }
    // 初始化光源
    {
        using DirectX::XMFLOAT3;
        // 点光源（索引0）
        m_Lights[0].type     = 1;
        m_Lights[0].enabled  = m_PointLightEnabled ? 1 : 0;
        m_Lights[0].position = XMFLOAT3(0.0f, 10.0f, 0.0f);
        m_Lights[0].range    = 30.0f;
        m_Lights[0].direction= XMFLOAT3(0.0f, -1.0f, 0.0f);
        m_Lights[0].spot     = DirectX::XM_PIDIV4; // unused
        m_Lights[0].ambient  = XMFLOAT3(0.05f, 0.05f, 0.05f);
        m_Lights[0].diffuse  = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_Lights[0].specular = XMFLOAT3(1.0f, 1.0f, 1.0f);
        // 聚光灯（索引1）
        m_Lights[1].type     = 2;
        m_Lights[1].enabled  = m_SpotLightEnabled ? 1 : 0;
        m_Lights[1].position = m_CameraPos; // 将在绘制时更新
        m_Lights[1].direction= XMFLOAT3(0.0f, 0.0f, 1.0f);
        m_Lights[1].range    = 80.0f;
        m_Lights[1].spot     = DirectX::XMConvertToRadians(30.0f); // 30度
        m_Lights[1].ambient  = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_Lights[1].diffuse  = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_Lights[1].specular = XMFLOAT3(1.0f, 1.0f, 1.0f);
        // 方向光（索引2）
        m_Lights[2].type     = 0;
        m_Lights[2].enabled  = m_DirLightEnabled ? 1 : 0;
        // 随机化方向以避免每次都一致
        {
            float rx = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
            float ry = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
            float rz = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
            DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(rx, ry, rz, 0.0f));
            DirectX::XMStoreFloat3(&m_Lights[2].direction, dir);
        }
        m_Lights[2].position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_Lights[2].range    = 0.0f;
        m_Lights[2].spot     = 0.0f;
        m_Lights[2].ambient  = XMFLOAT3(0.1f, 0.1f, 0.1f);
        m_Lights[2].diffuse  = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_Lights[2].specular = XMFLOAT3(1.0f, 1.0f, 1.0f);
    }

    return true;
}

//

void GameApp::UpdateCameraForCube()
{
    float halfExtent = (m_N - 1) * m_Spacing * 0.5f;
    float radius = std::max<float>(halfExtent * 1.732051f + 8.0f, 12.0f);

    XMFLOAT3 camPos(0.0f, radius * 0.45f, -radius * 1.3f);

    XMVECTOR eyePos = XMLoadFloat3(&camPos);
    XMMATRIX view = XMMatrixLookAtLH(
        eyePos,
        XMVectorZero(),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    float znear = 1.0f;
    float zfar = std::max<float>(1000.0f, radius * 6.0f);

    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4 * 1.2f, AspectRatio(), znear, zfar);

    m_CBuffer.view = XMMatrixTranspose(view);
    m_CBuffer.proj = XMMatrixTranspose(proj);

    XMStoreFloat3(&m_CameraPos, eyePos);
    XMVECTOR direction = XMVector3Normalize(XMVectorSubtract(XMVectorZero(), eyePos));
    m_CameraYaw = std::atan2(XMVectorGetX(direction), XMVectorGetZ(direction));
    float dirY = XMVectorGetY(direction);
    dirY = std::max(-1.0f, std::min(1.0f, dirY));
    m_CameraPitch = std::asin(dirY);
    m_CameraRoll = 0.0f;
}

// ==== 许双博第三次作业修改：根据键鼠输入更新飞行相机 ====
void GameApp::UpdateFlightCamera(float dt)
{
    using namespace DirectX;

    // 鼠标控制偏航和俯仰
    if (!m_FirstMouseEvent)
    {
        m_CameraYaw += m_MouseDeltaX * m_MouseSensitivity;
        m_CameraPitch += m_MouseDeltaY * m_MouseSensitivity;
    }
    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;

    // 限制俯仰角避免翻折
    const float pitchLimit = XM_PIDIV2 - 0.01f;
    m_CameraPitch = std::max(-pitchLimit, std::min(pitchLimit, m_CameraPitch));

    // 键盘桶滚，左右滚转
    if (GetAsyncKeyState('Q') & 0x8000)
        m_CameraRoll -= m_RollSpeed * dt;
    if (GetAsyncKeyState('E') & 0x8000)
        m_CameraRoll += m_RollSpeed * dt;

    // 保持角度在 [-pi, pi] 范围，避免浮点漂移
    auto WrapAngle = [](float angle)
    {
        while (angle > XM_PI) angle -= XM_2PI;
        while (angle < -XM_PI) angle += XM_2PI;
        return angle;
    };
    m_CameraYaw = WrapAngle(m_CameraYaw);
    m_CameraRoll = WrapAngle(m_CameraRoll);

    XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(m_CameraPitch, m_CameraYaw, m_CameraRoll);
    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
    XMVECTOR right = XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotation);
    XMVECTOR up = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation));

    XMVECTOR position = XMLoadFloat3(&m_CameraPos);
    float moveSpeed = m_MoveSpeed;

    if (GetAsyncKeyState('W') & 0x8000)
        position += forward * moveSpeed * dt;
    if (GetAsyncKeyState('S') & 0x8000)
        position -= forward * moveSpeed * dt;
    if (GetAsyncKeyState('A') & 0x8000)
        position -= right * moveSpeed * dt;
    if (GetAsyncKeyState('D') & 0x8000)
        position += right * moveSpeed * dt;

    XMStoreFloat3(&m_CameraPos, position);

    XMMATRIX view = XMMatrixLookToLH(position, forward, up);
    m_CBuffer.view = XMMatrixTranspose(view);
}



void GameApp::UpdateFirstPersonCamera(float dt)
{
    UpdatePlayerMovement(dt);
    UpdateFirstPersonViewMatrix();
}

void GameApp::UpdateThirdPersonCamera(float dt)
{
    UpdatePlayerMovement(dt);
    UpdateThirdPersonViewMatrix();
}

void GameApp::UpdatePlayerMovement(float dt)
{
    using namespace DirectX;

    if (!m_FirstMouseEvent)
    {
        m_PlayerYaw += m_MouseDeltaX * m_MouseSensitivity;
        m_PlayerPitch += m_MouseDeltaY * m_MouseSensitivity;
    }
    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;

    const float pitchLimit = XM_PIDIV2 - 0.01f;
    m_PlayerPitch = std::max(-pitchLimit, std::min(pitchLimit, m_PlayerPitch));

    auto WrapAngle = [](float angle)
    {
        while (angle > XM_PI) angle -= XM_2PI;
        while (angle < -XM_PI) angle += XM_2PI;
        return angle;
    };
    m_PlayerYaw = WrapAngle(m_PlayerYaw);

    XMVECTOR forward = GetForwardVector(m_PlayerYaw, m_PlayerPitch);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

    XMVECTOR position = XMLoadFloat3(&m_PlayerPos);
    float moveSpeed = m_PlayerMoveSpeed;

    if (GetAsyncKeyState('W') & 0x8000)
        position += forward * moveSpeed * dt;
    if (GetAsyncKeyState('S') & 0x8000)
        position -= forward * moveSpeed * dt;
    if (GetAsyncKeyState('A') & 0x8000)
        position -= right * moveSpeed * dt;
    if (GetAsyncKeyState('D') & 0x8000)
        position += right * moveSpeed * dt;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
        position += up * moveSpeed * dt;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        position -= up * moveSpeed * dt;

    XMStoreFloat3(&m_PlayerPos, position);
}
// ==== 许双博第三次作业修改：刷新飞行相机视图矩阵 ====
void GameApp::UpdateFreeFlightViewMatrix()
{
    using namespace DirectX;
    XMVECTOR position = XMLoadFloat3(&m_CameraPos);
    XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(m_CameraPitch, m_CameraYaw, m_CameraRoll);
    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
    XMVECTOR up = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation));
    XMMATRIX view = XMMatrixLookToLH(position, forward, up);
    m_CBuffer.view = XMMatrixTranspose(view);
}



void GameApp::UpdateFirstPersonViewMatrix()
{
    using namespace DirectX;
    XMVECTOR basePos = XMLoadFloat3(&m_PlayerPos);
    XMVECTOR eye = basePos + XMVectorSet(0.0f, m_PlayerEyeHeight, 0.0f, 0.0f);
    XMVECTOR forward = GetForwardVector(m_PlayerYaw, m_PlayerPitch);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMStoreFloat3(&m_FirstPersonEyePos, eye);
    XMMATRIX view = XMMatrixLookToLH(eye, forward, up);
    m_CBuffer.view = XMMatrixTranspose(view);
}

void GameApp::UpdateThirdPersonViewMatrix()
{
    using namespace DirectX;
    XMVECTOR basePos = XMLoadFloat3(&m_PlayerPos);
    XMVECTOR target = basePos + XMVectorSet(0.0f, m_PlayerEyeHeight, 0.0f, 0.0f);
    XMVECTOR forward = GetForwardVector(m_PlayerYaw, m_PlayerPitch);
    XMVECTOR cameraPos = target - forward * m_ThirdPersonDistance + XMVectorSet(0.0f, m_ThirdPersonHeight, 0.0f, 0.0f);
    XMStoreFloat3(&m_ThirdPersonCameraPos, cameraPos);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(cameraPos, target, up);
    m_CBuffer.view = XMMatrixTranspose(view);
}

void GameApp::ApplyViewMatrix()
{
    switch (m_CameraMode)
    {
    case CameraMode::AutoFit:
        UpdateCameraForCube();
        break;
    case CameraMode::FirstPerson:
        UpdateFirstPersonViewMatrix();
        break;
    case CameraMode::ThirdPerson:
        UpdateThirdPersonViewMatrix();
        break;
    case CameraMode::FreeFlight:
    default:
        UpdateFreeFlightViewMatrix();
        break;
    }
}

DirectX::XMVECTOR GameApp::GetForwardVector(float yaw, float pitch) const
{
    using namespace DirectX;
    XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(pitch, yaw, 0.0f);
    return XMVector3Normalize(XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation));
}

void GameApp::SetCameraMode(CameraMode mode)
{
    if (m_CameraMode == mode)
        return;

    using namespace DirectX;

    auto alignFromCurrentView = [this]()
    {
        XMVECTOR camPos = XMLoadFloat3(&m_CameraPos);
        XMVECTOR forward = GetForwardVector(m_CameraYaw, m_CameraPitch);
        XMVECTOR target = camPos + forward * m_ThirdPersonDistance;
        target -= XMVectorSet(0.0f, m_ThirdPersonHeight, 0.0f, 0.0f);
        target -= XMVectorSet(0.0f, m_PlayerEyeHeight, 0.0f, 0.0f);
        XMStoreFloat3(&m_PlayerPos, target);
        m_PlayerYaw = m_CameraYaw;
        m_PlayerPitch = m_CameraPitch;
    };

    if (mode == CameraMode::FirstPerson)
    {
        if (m_CameraMode == CameraMode::FreeFlight || m_CameraMode == CameraMode::AutoFit)
        {
            m_PlayerYaw = m_CameraYaw;
            m_PlayerPitch = m_CameraPitch;
            m_PlayerPos = m_CameraPos;
            m_PlayerPos.y -= m_PlayerEyeHeight;
        }
    }
    else if (mode == CameraMode::ThirdPerson)
    {
        if (m_CameraMode == CameraMode::FreeFlight || m_CameraMode == CameraMode::AutoFit)
        {
            alignFromCurrentView();
        }
    }
    else if (mode == CameraMode::FreeFlight)
    {
        if (m_CameraMode == CameraMode::FirstPerson)
        {
            m_CameraPos = m_FirstPersonEyePos;
            m_CameraYaw = m_PlayerYaw;
            m_CameraPitch = m_PlayerPitch;
        }
        else if (m_CameraMode == CameraMode::ThirdPerson)
        {
            m_CameraPos = m_ThirdPersonCameraPos;
            m_CameraYaw = m_PlayerYaw;
            m_CameraPitch = m_PlayerPitch;
        }
    }

    if (mode != CameraMode::FreeFlight)
    {
        m_CameraRoll = 0.0f;
    }

    m_CameraMode = mode;
    m_FirstMouseEvent = true;
    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;

    if (mode == CameraMode::AutoFit)
    {
        UpdateCameraForCube();
    }
    else
    {
        UpdateProjectionMatrix();
        ApplyViewMatrix();
    }
}

bool GameApp::IsMouseLookEnabled() const
{
    return m_CameraMode != CameraMode::AutoFit;
}
// ==== 许双博第三次作业修改：刷新飞行相机投影矩阵 ====
void GameApp::UpdateProjectionMatrix()
{
    using namespace DirectX;
    float aspect = AspectRatio();
    float fov = XM_PIDIV4 * 1.1f;
    m_CBuffer.proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fov, aspect, 0.1f, 2000.0f));
}

// ==== 许双博第三次作业修改：处理鼠标消息，记录移动增量 ====
LRESULT GameApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_MOUSEMOVE:
        if (IsMouseLookEnabled())
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (m_FirstMouseEvent)
            {
                m_LastMousePos.x = x;
                m_LastMousePos.y = y;
                m_FirstMouseEvent = false;
            }
            else
            {
                m_MouseDeltaX += float(x - m_LastMousePos.x);
                m_MouseDeltaY += float(y - m_LastMousePos.y);
                m_LastMousePos.x = x;
                m_LastMousePos.y = y;
            }
        }
        return 0;
    case WM_SETFOCUS:
        m_FirstMouseEvent = true;
        m_MouseDeltaX = 0.0f;
        m_MouseDeltaY = 0.0f;
        return 0;
    }
    return D3DApp::MsgProc(hwnd, msg, wParam, lParam);
}
