#include "GerstnerWaveManager.h"

AGerstnerWaveManager::AGerstnerWaveManager()
{
    PrimaryActorTick.bCanEverTick = true;

    OceanMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("OceanMesh"));
    RootComponent = OceanMesh;
    OceanMesh->bUseAsyncCooking = true;

    // --- 优化的波浪参数 (打破重复感) ---
    // 技巧：使用"非倍数"的波长 (质数)，方向也要这指一下那指一下

    // 1. 主涌浪 (Main Swell) - 负责大起伏
    FGerstnerWave W1; W1.Direction = FVector2D(1.0f, 0.1f);   W1.Wavelength = 512.0f; W1.Steepness = 0.12f; W1.Amplitude = 25.0f;
    Waves.Add(W1);

    // 2. 次级浪 (Secondary) - 稍微偏一点方向
    FGerstnerWave W2; W2.Direction = FVector2D(0.7f, 0.5f);   W2.Wavelength = 233.0f; W2.Steepness = 0.12f; W2.Amplitude = 12.0f;
    Waves.Add(W2);

    // 3. 侧向干扰浪 (Crossing Swell) - 用来打破纵向的条纹感
    FGerstnerWave W3; W3.Direction = FVector2D(-0.4f, 0.8f);  W3.Wavelength = 113.0f; W3.Steepness = 0.10f; W3.Amplitude = 6.0f;
    Waves.Add(W3);

    // 4. 反向小碎浪 - 增加混乱度
    FGerstnerWave W4; W4.Direction = FVector2D(0.2f, -0.6f);  W4.Wavelength = 67.0f;  W4.Steepness = 0.10f; W4.Amplitude = 3.0f;
    Waves.Add(W4);

    // --- 增加更多细节层 (为了让它不那么像低模) ---

    // 5. 细节 A
    FGerstnerWave W5; W5.Direction = FVector2D(-0.6f, -0.2f); W5.Wavelength = 41.0f;  W5.Steepness = 0.08f; W5.Amplitude = 1.5f;
    Waves.Add(W5);

    // 6. 细节 B
    FGerstnerWave W6; W6.Direction = FVector2D(0.8f, -0.4f);  W6.Wavelength = 23.0f;  W6.Steepness = 0.08f; W6.Amplitude = 0.8f;
    Waves.Add(W6);

    // 7. 细节 C
    FGerstnerWave W7; W7.Direction = FVector2D(-0.3f, 0.9f);  W7.Wavelength = 13.0f;  W7.Steepness = 0.06f; W7.Amplitude = 0.4f;
    Waves.Add(W7);

    // 8. 细节 D
    FGerstnerWave W8; W8.Direction = FVector2D(0.5f, 0.3f);   W8.Wavelength = 7.0f;   W8.Steepness = 0.05f; W8.Amplitude = 0.2f;
    Waves.Add(W8);
}

void AGerstnerWaveManager::BeginPlay()
{
    Super::BeginPlay();
    GenerateGrid();
}

void AGerstnerWaveManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 获取时间
    float Time = GetWorld()->GetTimeSeconds() * TimeScale;

    // 更新波浪物理
    UpdateWaves(Time);
}

void AGerstnerWaveManager::GenerateGrid()
{
    Vertices.Reset();
    Triangles.Reset();
    UVs.Reset();
    Normals.Reset();
    Tangents.Reset();
    Colors.Reset();

    // 生成 N+1 个点以闭合网格
    int32 NumVerts = MeshResolution + 1;
    float StepSize = OceanSize / MeshResolution;

    for (int32 m = 0; m < NumVerts; m++)
    {
        for (int32 n = 0; n < NumVerts; n++)
        {
            // 初始平面位置
            Vertices.Add(FVector(n * StepSize, m * StepSize, 0.0f));
            UVs.Add(FVector2D(n / (float)MeshResolution, m / (float)MeshResolution));

            // 默认法线和切线
            Normals.Add(FVector(0, 0, 1));
            Tangents.Add(FProcMeshTangent(1, 0, 0));
            Colors.Add(FColor::White);

            // 构建三角形索引
            if (n < MeshResolution && m < MeshResolution)
            {
                int32 Current = m * NumVerts + n;
                int32 Right = Current + 1;
                int32 Bottom = Current + NumVerts;
                int32 BottomRight = Bottom + 1;

                Triangles.Add(Current);
                Triangles.Add(Bottom);
                Triangles.Add(Right);

                Triangles.Add(Right);
                Triangles.Add(Bottom);
                Triangles.Add(BottomRight);
            }
        }
    }

    OceanMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);

    // 2. [新增] 应用材质
    // 检查用户是否在编辑器里选了材质，如果选了，就赋给 Section 0
    if (OceanMaterial)
    {
        OceanMesh->SetMaterial(0, OceanMaterial);
    }
}

void AGerstnerWaveManager::UpdateWaves(float Time)
{
    int32 NumVerts = MeshResolution + 1;

    // 1. 遍历所有顶点计算位移
    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        // 还原基础位置
        FVector BasePos = FVector(UVs[i].X * OceanSize, UVs[i].Y * OceanSize, 0.0f);
        FVector FinalPos = BasePos;
        FinalPos.Z = 0;

        for (const FGerstnerWave& W : Waves)
        {
            // 防止除以0
            float Wavelength = FMath::Max(W.Wavelength, 10.0f);
            float k = 2.0f * PI / Wavelength;
            float c = FMath::Sqrt(9.81f / k);
            FVector2D Dir = W.Direction.GetSafeNormal();

            float Dot = FVector2D::DotProduct(Dir, FVector2D(BasePos.X, BasePos.Y));
            float Theta = k * (Dot - c * Time);

            float SinVal = FMath::Sin(Theta);
            float CosVal = FMath::Cos(Theta);

            // Z 轴位移 (高度)
            FinalPos.Z += W.Amplitude * SinVal;

            // --- [关键修复 1] 形状修复 ---
            // 之前的公式 (Steepness/k) * Amplitude 导致了单位平方级的爆炸
            // 现在改为直接用 steepness 作为 0~1 的系数
            // 只要 Steepness * Amplitude < Wavelength / 2PI，就不会打结
            // 这里我们简单粗暴地用乘法，保证不炸
            float HorizontalOffset = W.Steepness * W.Amplitude * CosVal;

            FinalPos.X -= Dir.X * HorizontalOffset;
            FinalPos.Y -= Dir.Y * HorizontalOffset;
        }
        Vertices[i] = FinalPos;
    }

    // 2. 重新计算法线
    for (int32 m = 0; m < NumVerts; m++)
    {
        for (int32 n = 0; n < NumVerts; n++)
        {
            int32 Index = m * NumVerts + n;

            // 寻找邻居 (处理边缘)
            int32 n_next = (n == NumVerts - 1) ? n - 1 : n + 1;
            int32 m_next = (m == NumVerts - 1) ? m - 1 : m + 1;

            int32 IndexRight = m * NumVerts + n_next;
            int32 IndexBottom = m_next * NumVerts + n;

            FVector P_Current = Vertices[Index];
            FVector P_Right = Vertices[IndexRight];
            FVector P_Bottom = Vertices[IndexBottom];

            FVector V_Horizontal = P_Right - P_Current;
            FVector V_Vertical = P_Bottom - P_Current;

            // 边缘修正：如果是往回找的点，向量方向要反过来
            if (n == NumVerts - 1) V_Horizontal *= -1.0f;
            if (m == NumVerts - 1) V_Vertical *= -1.0f;

            // --- [关键修复 2] 颜色/法线修复 ---
            // 之前的 CrossProduct(V_Vertical, V_Horizontal) 算出来是朝下的 (0,0,-1)
            // 所以海面是黑的。
            // 现在交换顺序：Horizontal x Vertical = Up (0,0,1)
            FVector NewNormal = FVector::CrossProduct(V_Horizontal, V_Vertical).GetSafeNormal();
            Normals[Index] = NewNormal;
        }
    }

    // 3. 提交更新
    OceanMesh->UpdateMeshSection(0, Vertices, Normals, UVs, Colors, Tangents);
}