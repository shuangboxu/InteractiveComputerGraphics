#ifndef GAMEAPP_H
#define GAMEAPP_H

#include "d3dApp.h"
#include <array>
#include <vector>
// ==== 许双博第三次作业修改：飞行相机需要用到窗口结构和鼠标宏 ==== // 小组成员修改
#include <Windows.h>
#include <windowsx.h>
#include <DirectXMath.h> // 小组成员修改

class NameVertices; // 声明类

class GameApp : public D3DApp
{
public:
    struct VertexPosColor // 小组成员修改
    {
        DirectX::XMFLOAT3 pos; // 小组成员修改
        DirectX::XMFLOAT4 color; // 小组成员修改
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
    };

    struct ConstantBuffer // 小组成员修改
    {
        DirectX::XMMATRIX world; // 小组成员修改
        DirectX::XMMATRIX view; // 小组成员修改
        DirectX::XMMATRIX proj; // 小组成员修改
    };

public:
    GameApp(HINSTANCE hInstance);
    ~GameApp();

    bool Init();
    void OnResize();
    void UpdateScene(float dt);
    void DrawScene();
    // ==== 许双博第三次作业修改：处理鼠标消息，收集相机输入 ====
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override; // 小组成员修改

private:
    bool InitEffect();
    bool InitResource();
    // ==== 许双博第三次作业修改：飞行相机辅助函数 ==== // 小组成员修改
    void UpdateFlightCamera(float dt); // 小组成员修改
    void UpdatePlayerCamera(float dt); // 小组成员修改
    void UpdateViewMatrix(); // 小组成员修改
    void UpdateProjectionMatrix(); // 小组成员修改

private:
    enum class CameraMode // 小组成员修改
    {
        FreeFlight,
        FirstPerson,
        ThirdPerson
    };

    ComPtr<ID3D11InputLayout>   m_pVertexLayout;
    // ==== 改成 4 组 VB/IB（四个字各一份） ==== // 小组成员修改
    std::array<ComPtr<ID3D11Buffer>, 4> m_pVertexBuffers; // 小组成员修改
    std::array<ComPtr<ID3D11Buffer>, 4> m_pIndexBuffers; // 小组成员修改
    std::array<UINT, 4>                 m_IndexCounts; // 小组成员修改

    ComPtr<ID3D11Buffer>        m_pConstantBuffer;

    ComPtr<ID3D11VertexShader>  m_pVertexShader;
    ComPtr<ID3D11PixelShader>   m_pPixelShader;
    ConstantBuffer              m_CBuffer; // 小组成员修改

    // ==== 4 个模型对象缓存 ====
    std::array<NameVertices*, 4> m_Models = { nullptr, nullptr, nullptr, nullptr }; // 小组成员修改

    float   angle = 0.0f;

    // 立方体阵列控制
    int     m_N = 2; // 小组成员修改
    float   m_Spacing = 4.5f; // 小组成员修改
    float   m_OrbitRadius = 2.5f; // 小组成员修改
    int     m_OrbitMin = 1; // 小组成员修改
    int     m_OrbitMax = 3; // 小组成员修改
    CameraMode m_CameraMode = CameraMode::FreeFlight; // 小组成员修改
    float   m_KeyCooldown = 0.0f; // 小组成员修改

    // ==== 许双博第三次作业修改：飞行相机状态 ====
    DirectX::XMFLOAT3 m_CameraPos = DirectX::XMFLOAT3(0.0f, 20.0f, -80.0f); // 小组成员修改
    float   m_CameraYaw = 0.0f; // 小组成员修改
    float   m_CameraPitch = -0.25f; // 小组成员修改
    float   m_CameraRoll = 0.0f; // 小组成员修改
    float   m_MoveSpeed = 35.0f; // 小组成员修改
    float   m_RollSpeed = DirectX::XMConvertToRadians(60.0f); // 小组成员修改
    float   m_MouseSensitivity = 0.0025f; // 小组成员修改
    float   m_MouseDeltaX = 0.0f; // 小组成员修改
    float   m_MouseDeltaY = 0.0f; // 小组成员修改
    POINT   m_LastMousePos = { 0, 0 }; // 小组成员修改
    bool    m_FirstMouseEvent = true; // 小组成员修改

    // 玩家（“你”）模型状态
    DirectX::XMFLOAT3 m_PlayerPos = DirectX::XMFLOAT3(0.0f, 0.0f, -60.0f); // 小组成员修改
    float   m_PlayerYaw = 0.0f; // 小组成员修改
    float   m_PlayerPitch = 0.0f; // 小组成员修改
    float   m_PlayerMoveSpeed = 22.0f; // 小组成员修改
    float   m_PlayerEyeHeight = 8.0f; // 小组成员修改
    float   m_PlayerScale = 1.0f; // 小组成员修改
    float   m_ThirdPersonDistance = 25.0f; // 小组成员修改
    float   m_ThirdPersonHeight = 8.0f; // 小组成员修改

    ComPtr<ID3D11Buffer> m_pPlayerVertexBuffer; // 小组成员修改
    ComPtr<ID3D11Buffer> m_pPlayerIndexBuffer; // 小组成员修改
    UINT    m_PlayerIndexCount = 0; // 小组成员修改
}; 

#endif
