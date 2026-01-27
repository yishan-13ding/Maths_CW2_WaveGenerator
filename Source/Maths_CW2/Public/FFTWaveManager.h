#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <complex> // 必须在文件顶部包含
#include <vector>
#include "ProceduralMeshComponent.h"
#include "FFTWaveManager.generated.h" //must be the last include

typedef std::complex<float> Complex;

UCLASS()
class MATHS_CW2_API AFFTWaveManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFFTWaveManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 计算菲利普能量谱函数
    float CalculatePhillips(FVector2D k);

    // --- 在这里开始添加波浪参数 ---

    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    int32 MeshResolution = 64; // FFT 分辨率 (N)

    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    float OceanSize = 1000.0f; // 海洋物理尺寸 (L)

    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    float TimeScale = 1.0f; // 时间流速控制

    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    float Amplitude = 1.0f; // 菲利普常数 (A)

    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    FVector2D WindDirection = FVector2D(1.0f, 1.0f); // 风向

    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    float WindSpeed = 20.0f; // 风速

    UPROPERTY(EditAnywhere, Category = "Wave Settings")
    UMaterialInterface* OceanMaterial;

    // 存储初始频谱数据和它的共轭（用于后续 FFT 计算）
    //std::vector<Complex> h0_tilde;
    //std::vector<Complex> h0_tilde_conj;
    TArray<Complex> h0_tilde;
    TArray<Complex> h0_tilde_conj;
    
    //把点连成面
    // 1. 可视化的网格组件
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UProceduralMeshComponent* OceanMesh;

    // 2. 网格数据缓存（必须保存为成员变量，因为 Tick 每一帧都要修改它）
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;
    TArray<FColor> Colors;

    // 辅助函数：生成网格的初始形状
    void GenerateGrid();

	//IFFT 相关函数
    void PerformIDFT_Row(int32 RowIndex, const TArray<Complex>& Input, TArray<Complex>& Output);
    void PerformIDFT_Col(int32 ColIndex, const TArray<Complex>& Input, TArray<Complex>& Output);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
