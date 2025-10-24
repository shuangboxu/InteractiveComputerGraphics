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

const D3D11_INPUT_ELEMENT_DESC GameApp::VertexPosColor::inputLayout[2] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
    UpdateProjectionMatrix();   // ==== 许双博第三次作业修改：保持飞行相机的投影矩阵 ====
    ApplyViewMatrix();          // ==== 视角更新 ====
}

void GameApp::UpdateScene(float dt)
{
    if (m_KeyCooldown > 0.0f) m_KeyCooldown -= dt;
    angle += 0.5f * dt;

#define CLAMP(v, lo, hi)  ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

    if (m_KeyCooldown <= 0.0f)
    {
        if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) { m_N = CLAMP(m_N + 1, 1, 200); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.20f; }
        else if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) { m_N = CLAMP(m_N - 1, 1, 200); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.20f; }
        else if (GetAsyncKeyState(VK_OEM_4) & 0x8000) { m_Spacing = std::max(1.0f, m_Spacing - 0.5f); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.10f; }
        else if (GetAsyncKeyState(VK_OEM_6) & 0x8000) { m_Spacing = std::min(20.0f, m_Spacing + 0.5f); if (m_CameraMode == CameraMode::AutoFit) UpdateCameraForCube(); m_KeyCooldown = 0.10f; }
        else if (GetAsyncKeyState(VK_OEM_COMMA) & 0x8000) { m_OrbitMax = std::max(0, m_OrbitMax - 1); m_OrbitMin = std::min(m_OrbitMin, m_OrbitMax); m_KeyCooldown = 0.10f; }
        else if (GetAsyncKeyState(VK_OEM_PERIOD) & 0x8000) { m_OrbitMax = std::min(6, m_OrbitMax + 1); m_KeyCooldown = 0.10f; }
        else if (GetAsyncKeyState('1') & 0x8000) { SetCameraMode(CameraMode::FirstPerson); m_KeyCooldown = 0.20f; }
        else if (GetAsyncKeyState('2') & 0x8000) { SetCameraMode(CameraMode::ThirdPerson); m_KeyCooldown = 0.20f; }
        else if (GetAsyncKeyState('3') & 0x8000) { SetCameraMode(CameraMode::FreeFlight); m_KeyCooldown = 0.20f; }
        else if (GetAsyncKeyState('0') & 0x8000) { SetCameraMode(CameraMode::AutoFit); m_KeyCooldown = 0.20f; }
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
    VertexPosColor playerVertices[] =
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

    WORD playerIndices[] =
    {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        4, 5, 1, 4, 1, 0,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        4, 0, 3, 4, 3, 7
    };

    D3D11_BUFFER_DESC playerVbd{};
    playerVbd.Usage = D3D11_USAGE_IMMUTABLE;
    playerVbd.ByteWidth = sizeof(playerVertices);
    playerVbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA playerVBData{};
    playerVBData.pSysMem = playerVertices;
    HR(m_pd3dDevice->CreateBuffer(&playerVbd, &playerVBData, m_pPlayerVertexBuffer.ReleaseAndGetAddressOf()));

    D3D11_BUFFER_DESC playerIbd{};
    playerIbd.Usage = D3D11_USAGE_IMMUTABLE;
    playerIbd.ByteWidth = sizeof(playerIndices);
    playerIbd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA playerIBData{};
    playerIBData.pSysMem = playerIndices;
    HR(m_pd3dDevice->CreateBuffer(&playerIbd, &playerIBData, m_pPlayerIndexBuffer.ReleaseAndGetAddressOf()));

    m_PlayerIndexCount = static_cast<UINT>(_countof(playerIndices));

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

    return true;
}

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
