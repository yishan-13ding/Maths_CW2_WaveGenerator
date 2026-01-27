#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "GerstnerWaveManager.generated.h"

// 定义单个波浪的参数结构体
USTRUCT(BlueprintType)
struct FGerstnerWave
{
    GENERATED_BODY()

    // 波浪传播方向 (X, Y)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Param")
    FVector2D Direction = FVector2D(1.0f, 0.0f);

    // 波长 (Wavelength): 决定波浪的宽度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Param")
    float Wavelength = 600.0f;

    // 陡度 (Steepness): 决定浪尖有多尖 (0~1)，过大会导致穿插
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Param", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Steepness = 0.2f;

    // 振幅 (Amplitude): 决定浪有多高
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave Param")
    float Amplitude = 20.0f;


};

UCLASS()
class MATHS_CW2_API AGerstnerWaveManager : public AActor
{
    GENERATED_BODY()

public:
    AGerstnerWaveManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // --- 核心组件 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UProceduralMeshComponent* OceanMesh;

    // --- 网格设置 (保持与 FFT 一致以便对比) ---
    UPROPERTY(EditAnywhere, Category = "Grid Settings")
    int32 MeshResolution = 64;

    UPROPERTY(EditAnywhere, Category = "Grid Settings")
    float OceanSize = 2000.0f;

    // --- 波浪设置 ---
    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    float TimeScale = 1.0f;

    // 定义 4 个基础波浪 (为了对比 FFT 的成千上万个波)
    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    TArray<FGerstnerWave> Waves;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean Visuals")
    UMaterialInterface* OceanMaterial;

private:
    // 网格数据
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    // 辅助函数
    void GenerateGrid();
    void UpdateWaves(float Time);
};