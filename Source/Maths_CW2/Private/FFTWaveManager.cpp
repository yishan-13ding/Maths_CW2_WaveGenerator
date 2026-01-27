#include "FFTWaveManager.h"
#include "DSP/FloatArrayMath.h" 
#include "DrawDebugHelpers.h"
//#include "DSP/FastFourierTransform.h"
//#include "DSP/FastFourierTransform.h"

//构造函数
AFFTWaveManager::AFFTWaveManager()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建组件并设为根节点
    OceanMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("OceanMesh"));
    RootComponent = OceanMesh;

    // 这是一个优化设置，为了让网格更新更快
    OceanMesh->bUseAsyncCooking = true;
}

// Called when the game starts or when spawned
void AFFTWaveManager::BeginPlay()
{
    Super::BeginPlay();
    GenerateGrid();

    // 1. 初始化数组大小
    int32 TotalSize = MeshResolution * MeshResolution;
    h0_tilde.Empty();
    h0_tilde.AddZeroed(TotalSize);
    h0_tilde_conj.Empty();
    h0_tilde_conj.AddZeroed(TotalSize);

    // 2. 开始双重循环计算
    for (int32 m = 0; m < MeshResolution; m++)
    {
        for (int32 n = 0; n < MeshResolution; n++)
        {
            // 计算波向量 k
            // 将网格坐标映射到 [-N/2, N/2] 区间
            float kx = (2.0f * PI * (n - MeshResolution / 2.0f)) / OceanSize;
            float ky = (2.0f * PI * (m - MeshResolution / 2.0f)) / OceanSize;
            FVector2D k(kx, ky);

            int32 Index = m * MeshResolution + n;

            // 获取 Phillips 能量值
            float P = CalculatePhillips(k);

            // 为了让海浪看起来自然，我们需要引入高斯噪声（随机性）
            // 这里简单模拟高斯随机数
            float r1 = FMath::Frac(FMath::Sin(Index * 12.9898f) * 43758.5453f);
            float r2 = FMath::Frac(FMath::Sin(Index * 78.233f) * 43758.5453f);
            float noise = FMath::Sqrt(-2.0f * FMath::Loge(FMath::Max(r1, 0.0001f)));

            // 计算 h0
            float Real = noise * FMath::Cos(2.0f * PI * r2) * FMath::Sqrt(P * 0.5f);
            float Imag = noise * FMath::Sin(2.0f * PI * r2) * FMath::Sqrt(P * 0.5f);

            h0_tilde[Index] = Complex(Real, Imag);

            // 计算共轭（用于对称性，这是 FFT 处理实数高度图的技巧）
            h0_tilde_conj[Index] = std::conj(h0_tilde[Index]);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("FFT Wave Initialized: %d points calculated."), TotalSize);
}




void AFFTWaveManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ==========================================
    // 1. 实时更新海浪基因 (让参数调整实时生效)
    // ==========================================
    
    h0_tilde.Reset();
    h0_tilde.AddZeroed(MeshResolution * MeshResolution);
    h0_tilde_conj.Reset();
    h0_tilde_conj.AddZeroed(MeshResolution * MeshResolution);

    // 找到 Tick 函数开头的这个双重循环，完全替换它
    for (int32 m = 0; m < MeshResolution; m++)
    {
        for (int32 n = 0; n < MeshResolution; n++)
        {
            // 1. 必须先定义 k，否则 CalculatePhillips(k) 会报错！
            float kx = (2.0f * PI * (n - MeshResolution / 2.0f)) / OceanSize;
            float ky = (2.0f * PI * (m - MeshResolution / 2.0f)) / OceanSize;
            FVector2D k(kx, ky);

            int32 Index = m * MeshResolution + n;

            // 2. 现在再调用 CalculatePhillips 就没问题了
            float P = CalculatePhillips(k);

            // 3. 使用哈希代替随机数 (解决抖动问题的核心)
            // 利用 sin 函数的混乱性，把 Index 转换成一个固定的 0~1 之间的小数
            float r1 = FMath::Frac(FMath::Sin(Index * 12.9898f) * 43758.5453f);
            float r2 = FMath::Frac(FMath::Sin(Index * 78.233f) * 43758.5453f);

            float noise = FMath::Sqrt(-2.0f * FMath::Loge(FMath::Max(r1, 0.0001f)));
            float Real = noise * FMath::Cos(2.0f * PI * r2) * FMath::Sqrt(P * 0.5f);
            float Imag = noise * FMath::Sin(2.0f * PI * r2) * FMath::Sqrt(P * 0.5f);

            h0_tilde[Index] = Complex(Real, Imag);
            h0_tilde_conj[Index] = std::conj(h0_tilde[Index]);
        }
    }

    // ==========================================
    // 2. 时间演化与 IFFT 计算
    // ==========================================
    float Time = GetWorld()->GetTimeSeconds() * TimeScale;
    TArray<Complex> h_tilde_t;
    h_tilde_t.SetNum(MeshResolution * MeshResolution);

    for (int32 m = 0; m < MeshResolution; m++)
    {
        for (int32 n = 0; n < MeshResolution; n++)
        {
            int32 Index = m * MeshResolution + n;

            float kx = (2.0f * PI * (n - MeshResolution / 2.0f)) / OceanSize;
            float ky = (2.0f * PI * (m - MeshResolution / 2.0f)) / OceanSize;
            float kMag = FMath::Sqrt(kx * kx + ky * ky);

            // 消除中心尖塔 (直流分量)
            if (kMag < 0.0001f)
            {
                h_tilde_t[Index] = Complex(0, 0);
                continue;
            }

            float Omega = FMath::Sqrt(9.81f * kMag);
            float Phase = Omega * Time;
            Complex ExpIPhase(FMath::Cos(Phase), FMath::Sin(Phase));
            Complex ExpINegPhase(FMath::Cos(-Phase), FMath::Sin(-Phase));

            h_tilde_t[Index] = h0_tilde[Index] * ExpIPhase + h0_tilde_conj[Index] * ExpINegPhase;
        }
    }

    // 执行 IDFT
    TArray<Complex> TempRowOutput;
    TempRowOutput.SetNum(MeshResolution * MeshResolution);
    TArray<Complex> FinalHeightField;
    FinalHeightField.SetNum(MeshResolution * MeshResolution);

    for (int32 m = 0; m < MeshResolution; m++) PerformIDFT_Row(m, h_tilde_t, TempRowOutput);
    for (int32 n = 0; n < MeshResolution; n++) PerformIDFT_Col(n, TempRowOutput, FinalHeightField);

    // ==========================================
    // 3. 应用高度与计算法线 (适配 65x65 网格)
    // ==========================================

    // [重要修正] 不要直接比较 Num，因为 Vertices 可能多一圈 (65x65 vs 64x64)
    // 只要确保 Vertices 足够多即可
    if (Vertices.Num() < FinalHeightField.Num()) return;

    int32 NumVerts = MeshResolution + 1; // 假设我们是 65

    // 第一步：先更新所有点的 Z 轴 (纯净高度场)
    // 注意：这里遍历的是 Vertices (65x65)，而不是 FinalHeightField
    for (int32 m = 0; m < NumVerts; m++)
    {
        for (int32 n = 0; n < NumVerts; n++)
        {
            int32 VertexIndex = m * NumVerts + n;

            // [核心逻辑] 映射 65 -> 64
            // 当 n=64 时，取模变成 0，实现无缝连接
            int32 fft_m = m % MeshResolution;
            int32 fft_n = n % MeshResolution;
            int32 FFTIndex = fft_m * MeshResolution + fft_n;

            // 安全检查
            if (FinalHeightField.IsValidIndex(FFTIndex) && Vertices.IsValidIndex(VertexIndex))
            {
                float Height = FinalHeightField[FFTIndex].real();
                Vertices[VertexIndex].Z = Height * 0.005f; // 浪高系数
            }
        }
    }

    // 第二步：基于纯净高度场计算法线，并应用偏移
    for (int32 m = 0; m < NumVerts; m++)
    {
        for (int32 n = 0; n < NumVerts; n++)
        {
            int32 Index = m * NumVerts + n;

            // --- A. 计算法线 ---
            float H_Current = Vertices[Index].Z;

            // 邻居索引逻辑 (适配 65x65)
            // 如果 n+1 超过了边界 (65)，就取模回到开头
            int32 RightN = (n + 1) % MeshResolution;
            // 注意：取高度时，我们要去 FFT 数据对应的"那个形状"里找
            // 但为了简单，我们直接在 Vertices 数组里找"逻辑上的右邻居"
            // 由于 Vertices 已经是 65x65 且首尾重合的，直接取 n+1 即可，除非 n=64

            // 更稳健的做法：直接算坐标偏移
            int32 IndexRight = m * NumVerts + ((n == MeshResolution) ? 1 : n + 1);
            int32 IndexBottom = ((m == MeshResolution) ? 1 : m + 1) * NumVerts + n;

            // 防止数组越界 (虽然理论上不会，但安全第一)
            if (!Vertices.IsValidIndex(IndexRight) || !Vertices.IsValidIndex(IndexBottom)) continue;

            float H_Right = Vertices[IndexRight].Z;
            float H_Bottom = Vertices[IndexBottom].Z;

            float Step = OceanSize / MeshResolution;
            FVector VectorRight(Step, 0, H_Right - H_Current);
            FVector VectorBottom(0, Step, H_Bottom - H_Current);

            FVector NewNormal = FVector::CrossProduct(VectorBottom, VectorRight).GetSafeNormal();
            Normals[Index] = NewNormal * -1.0f;

            // --- B. 应用 Choppiness (偏移 X/Y) ---
            float Choppiness = 1.5f;
            if (Choppiness > 0.0f)
            {
                // 恢复原始网格位置
                float OriginalX = n * Step;
                float OriginalY = m * Step;

                // 1. 计算原本想偏移多少
                float OffsetX = Normals[Index].X * Choppiness * Vertices[Index].Z * 0.5f;
                float OffsetY = Normals[Index].Y * Choppiness * Vertices[Index].Z * 0.5f;

                // 2. [关键修复] 限制偏移量，防止网格爆炸！
                // 偏移量绝对不能超过网格间距的一半，否则就会穿插
                float MaxOffset = Step * 0.4f; // 留一点安全距离 (0.4倍网格宽)

                OffsetX = FMath::Clamp(OffsetX, -MaxOffset, MaxOffset);
                OffsetY = FMath::Clamp(OffsetY, -MaxOffset, MaxOffset);

                // 3. 应用限制后的偏移
                Vertices[Index].X = OriginalX - OffsetX;
                Vertices[Index].Y = OriginalY - OffsetY;
            }
        }
    }

    // 4. 提交
    OceanMesh->UpdateMeshSection(0, Vertices, Normals, UVs, Colors, Tangents);
}




float AFFTWaveManager::CalculatePhillips(FVector2D k)
{
    float kLength = k.Size();
    if (kLength < 0.000001f) return 0.0f; // 避免除以 0

    float kLength2 = kLength * kLength;
    float kLength4 = kLength2 * kLength2;

    // L_constant = v^2 / g
    float L_constant = (WindSpeed * WindSpeed) / 9.81f;
    float L2 = L_constant * L_constant;

    // 方向性因子：波浪方向与风向的夹角
    FVector2D WindDir = WindDirection;
    WindDir.Normalize();
    FVector2D kDir = k;
    kDir.Normalize();
    float dot = FVector2D::DotProduct(kDir, WindDir);
    float dot2 = dot * dot;

    // Phillips 公式核心实现
    return Amplitude * (FMath::Exp(-1.0f / (kLength2 * L2)) / kLength4) * dot2;
}


// 生成初始网格
void AFFTWaveManager::GenerateGrid()
{
    Vertices.Reset();
    Triangles.Reset();
    UVs.Reset();
    Normals.Reset();
    Tangents.Reset();
    Colors.Reset();

    // [核心修改] 我们需要 N+1 个点来围成 N 个格子
    int32 NumVerts = MeshResolution + 1;
    float StepSize = OceanSize / MeshResolution;

    for (int32 m = 0; m < NumVerts; m++) // 注意这里是 NumVerts (即 <= MeshResolution)
    {
        for (int32 n = 0; n < NumVerts; n++)
        {
            Vertices.Add(FVector(n * StepSize, m * StepSize, 0.0f));

            // UV 坐标 (0~1)
            UVs.Add(FVector2D(n / (float)MeshResolution, m / (float)MeshResolution));
            Normals.Add(FVector(0, 0, 1));
            Tangents.Add(FProcMeshTangent(1, 0, 0));
            Colors.Add(FColor::White);

            // 生成三角形 (只在不到边界时生成)
            if (n < MeshResolution && m < MeshResolution)
            {
                // [注意] 现在的行宽是 NumVerts (65)，而不是 64
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

    if (OceanMesh)
    {
        OceanMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
        if (OceanMaterial) OceanMesh->SetMaterial(0, OceanMaterial);
    }
}



// --- 手写 IDFT 算法 (分离轴法，O(N^3) 复杂度，对 64x64 网格足够快) ---

void AFFTWaveManager::PerformIDFT_Row(int32 RowIndex, const TArray<Complex>& Input, TArray<Complex>& Output)
{
    // 对每一行做 1D IDFT
    for (int32 x = 0; x < MeshResolution; x++)
    {
        Complex Sum(0, 0);
        for (int32 k = 0; k < MeshResolution; k++)
        {
            // 获取频谱数据 (Input 是 h_tilde_t)
            int32 InputIndex = RowIndex * MeshResolution + k;

            // 欧拉公式：e^(i * 2 * PI * k * x / N)
            float Angle = 2.0f * PI * k * x / MeshResolution;
            Complex Exp(FMath::Cos(Angle), FMath::Sin(Angle));

            Sum += Input[InputIndex] * Exp;
        }
        // 存入输出
        Output[RowIndex * MeshResolution + x] = Sum;
    }
}

void AFFTWaveManager::PerformIDFT_Col(int32 ColIndex, const TArray<Complex>& Input, TArray<Complex>& Output)
{
    // 对每一列做 1D IDFT
    for (int32 y = 0; y < MeshResolution; y++)
    {
        Complex Sum(0, 0);
        for (int32 k = 0; k < MeshResolution; k++)
        {
            // 注意这里是把 Row 的结果当作输入
            int32 InputIndex = k * MeshResolution + ColIndex;

            float Angle = 2.0f * PI * k * y / MeshResolution;
            Complex Exp(FMath::Cos(Angle), FMath::Sin(Angle));

            Sum += Input[InputIndex] * Exp;
        }
        Output[y * MeshResolution + ColIndex] = Sum;
    }
}