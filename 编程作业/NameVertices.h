#pragma once
#include "GameApp.h"

// 支持多汉字：id=0/1/2/3 对应四个不同名字
class NameVertices
{
public:
    // ==== 许双博改的：增加 id，选择要加载的汉字 ====
    NameVertices(D3D11_PRIMITIVE_TOPOLOGY type, int id);
    ~NameVertices();

    // 数据访问
    GameApp::VertexPosColor* GetNameVertices();
    WORD* GetNameIndices();
    D3D11_PRIMITIVE_TOPOLOGY GetTopology();
    UINT GetVerticesCount();
    UINT GetIndexCount();

private:
    GameApp::VertexPosColor* nameVertices = nullptr; // 顶点
    WORD* nameIndices = nullptr;                     // 索引
    D3D11_PRIMITIVE_TOPOLOGY topology;               // 图元类型
    UINT verticesCount = 0;                          // 顶点个数
    UINT indexCount = 0;                             // 索引个数
};
