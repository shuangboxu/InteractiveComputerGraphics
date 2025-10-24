#ifndef GAMEAPP_H
#define GAMEAPP_H

#include "d3dApp.h"
#include <array>        // ==== 许双博改的 ====
#include <vector>       // ==== 许双博改的 ====
// ==== 许双博第三次作业修改：飞行相机需要用到窗口结构和鼠标宏 ====
#include <Windows.h>
#include <windowsx.h>

class NameVertices; // 声明类

class GameApp : public D3DApp
{
public:
    struct VertexPosColor
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
    };

    struct ConstantBuffer
    {
        DirectX::XMMATRIX world;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX proj;
    };

public:
    GameApp(HINSTANCE hInstance);
    ~GameApp();

    bool Init();
    void OnResize();
    void UpdateScene(float dt);
    void DrawScene();
    // ==== 许双博第三次作业修改：处理鼠标消息，收集相机输入 ====
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    bool InitEffect();
    bool InitResource();
    void UpdateCameraForCube();   // ==== 许双博改的 ====
    // ==== 许双博第三次作业修改：飞行相机辅助函数 ====
    void UpdateFlightCamera(float dt);
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();

private:
    ComPtr<ID3D11InputLayout>   m_pVertexLayout;
    // ==== 许双博改的：改成 4 组 VB/IB（四个字各一份） ====
    std::array<ComPtr<ID3D11Buffer>, 4> m_pVertexBuffers;
    std::array<ComPtr<ID3D11Buffer>, 4> m_pIndexBuffers;
    std::array<UINT, 4>                 m_IndexCounts;

    ComPtr<ID3D11Buffer>        m_pConstantBuffer;

    ComPtr<ID3D11VertexShader>  m_pVertexShader;
    ComPtr<ID3D11PixelShader>   m_pPixelShader;
    ConstantBuffer              m_CBuffer;

    // ==== 许双博改的：4 个模型对象缓存 ====
    std::array<NameVertices*, 4> m_Models = { nullptr, nullptr, nullptr, nullptr };

    float   angle = 0.0f;

    // 立方体阵列控制
    int     m_N = 2;              // ==== 许双博改的 ====
    float   m_Spacing = 4.5f;     // ==== 许双博改的 ====
    float   m_OrbitRadius = 2.5f; // ==== 许双博改的 ====
    int     m_OrbitMin = 1;       // ==== 许双博改的 ====
    int     m_OrbitMax = 3;       // ==== 许双博改的 ====
    bool    m_AutoFitCamera = false; // ==== 许双博第三次作业修改：默认启用飞行相机 ====
    float   m_KeyCooldown = 0.0f;   // ==== 许双博改的 ====

    // ==== 许双博第三次作业修改：飞行相机状态 ====
    DirectX::XMFLOAT3 m_CameraPos = DirectX::XMFLOAT3(0.0f, 20.0f, -80.0f);
    float   m_CameraYaw = 0.0f;
    float   m_CameraPitch = -0.25f;
    float   m_CameraRoll = 0.0f;
    float   m_MoveSpeed = 35.0f;
    float   m_RollSpeed = DirectX::XMConvertToRadians(60.0f);
    float   m_MouseSensitivity = 0.0025f;
    float   m_MouseDeltaX = 0.0f;
    float   m_MouseDeltaY = 0.0f;
    POINT   m_LastMousePos = { 0, 0 };
    bool    m_FirstMouseEvent = true;
};

#endif
