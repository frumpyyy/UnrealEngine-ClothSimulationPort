#include "ClothActor.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "RHIGPUReadback.h"

#include "Shader_XPBDCloth.h"
#include "Shader_Integrate.h"
#include "Shader_ResetLambda.h"
#include "Shader_ApplyForce.h"
#include "Shader_Commons.h"
#include "Shader_ComputeNormals.h"
#include "Shader_TextureWrite.h"
#include "Shader_DebugPoints.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

// Sets default values
AClothActor::AClothActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ClothMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ClothMesh"));
	RootComponent = ClothMeshComp;
	ClothMeshComp->SetVisibility(true);
	ClothMeshComp->SetHiddenInGame(false);
	ClothMeshComp->SetBoundsScale(10.0f);
}

// Called when the game starts or when spawned
void AClothActor::BeginPlay()
{
	Super::BeginPlay();

	BuildParticles();
	BuildSprings();
	InitGPUSprings();
	BuildClothMesh();
	UE_LOG(LogTemp, Warning, TEXT("Mesh sections: %d"), ClothMeshComp->GetNumSections());
	UE_LOG(LogTemp, Warning, TEXT("Actor location: %s"), *GetActorLocation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("Cloth dimensions: %d x %d"), clothWidth, clothHeight);
	InitRendering();

	UE_LOG(LogTemp, Warning, TEXT("Particles: %d"), particles.Num());
	UE_LOG(LogTemp, Warning, TEXT("Springs: %d"), springs.Num());



}

void AClothActor::OnConstruction(const FTransform& Transform) {
	Super::OnConstruction(Transform);
	RebuildCloth();
}

#if WITH_EDITOR
void AClothActor::PostEditChangeProperty(FPropertyChangedEvent& propertyChanged) {
	Super::PostEditChangeProperty(propertyChanged);
	RebuildCloth();
}
#endif

// Called every frame
void AClothActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	timeAccumulator += DeltaTime;

	while (timeAccumulator >= FixedTimestep) {
		Simulate(FixedTimestep);
		timeAccumulator -= FixedTimestep;
	}

	if (DebugParticleBufferPositions)
		for (const FGPUParticle& P : particles)
		{
			DrawDebugPoint(GetWorld(), (FVector)P.position, 8.f, FColor::Red, false, -1.f, 0);
		}

	if (DebugParticleWPOPositions)
		for (const FVector3f& P : LatestDebugPoints)
		{
			DrawDebugPoint(GetWorld(), (FVector)P, 10.f, FColor::Green, false, -1.f, 0);
		}
}

void AClothActor::Simulate(float deltaTime) {
	if (!PendingReadback)
	{
		ENQUEUE_RENDER_COMMAND(RunCloth)(
			[this, deltaTime](FRHICommandListImmediate& RHIcmdList)
			{
#pragma region BufferInit

				FRDGBuilder GraphBuilder(RHIcmdList);

				FRDGBufferRef ParticleBuffer = CreateStructuredBuffer(
					GraphBuilder,
					TEXT("Particles"),
					sizeof(FGPUParticle),
					particles.Num(),
					particles.GetData(),
					sizeof(FGPUParticle) * particles.Num()
				);

				FRDGBufferRef SpringBuffer = CreateStructuredBuffer(
					GraphBuilder,
					TEXT("Springs"),
					sizeof(FGPUSpring),
					GPUSprings.Num(),
					GPUSprings.GetData(),
					sizeof(FGPUSpring) * GPUSprings.Num()
				);

				FRDGBufferRef NormalsBuffer = CreateStructuredBuffer(
					GraphBuilder,
					TEXT("Normals"),
					sizeof(FVector3f),
					particles.Num(),
					nullptr,
					0
				);

				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NormalsBuffer), 0.0f); //safely inits to 0 instead of def

				const uint32 threadGroupSize = 256;
				const uint32 particleCount = particles.Num();
				const uint32 springCount = springs.Num();
				const uint32 particleGroupCount = FMath::DivideAndRoundUp(particleCount, threadGroupSize);
				const uint32 springGroupCount = FMath::DivideAndRoundUp(springCount, threadGroupSize);

#pragma endregion

#pragma region RenderPasses

				for (int i = 0; i < numSubsteps; i++) {

					FResetLambdaShaderInterface::AddPass_RenderThread(GraphBuilder, springCount, springGroupCount, GetGlobalShaderMap(GMaxRHIFeatureLevel), SpringBuffer);

					FNormalsShaderInterface::AddPass_RenderThread(GraphBuilder, clothWidth, clothHeight, particleCount, deltaTime, particleGroupCount, GetGlobalShaderMap(GMaxRHIFeatureLevel), ParticleBuffer, NormalsBuffer);

					FApplyForceShaderInterface::AddPass_RenderThread(GraphBuilder, particleCount, Gravity, particleGroupCount, GetGlobalShaderMap(GMaxRHIFeatureLevel), ParticleBuffer);

					FIntegrateShaderInterface::AddPass_RenderThread(GraphBuilder, particleCount, numSubsteps, deltaTime, particleGroupCount, GetGlobalShaderMap(GMaxRHIFeatureLevel), ParticleBuffer);

					for (int x = 0; x < numIterations; x++) {

						for (int32 c = 0; c < COLOURCOUNT; c++)
						{
							if (colourCount[c] == 0)
							{
								continue;
							}

							FSolveSpringsShaderInterface::AddPass_RenderThread(GraphBuilder, colourOffsets[c], colourCount[c], numSubsteps, deltaTime, particleGroupCount, GetGlobalShaderMap(GMaxRHIFeatureLevel),
								ParticleBuffer, SpringBuffer);
						}
					}


				}




#pragma endregion

#pragma region TextureWriting

				FRDGTextureRef PositionTexture = RegisterExternalTexture(GraphBuilder, PositionRenderTarget->GetRenderTargetResource()->GetRenderTargetTexture(), TEXT("ClothPositionRenderTarget"));
				FRDGTextureRef NormalTexture = RegisterExternalTexture(GraphBuilder, NormalsRenderTarget->GetRenderTargetResource()->GetRenderTargetTexture(), TEXT("ClothNormalsRenderTarget"));

				FTextureWriteShaderInterface::AddPass_RenderThread(GraphBuilder, clothWidth, clothHeight, GetGlobalShaderMap(GMaxRHIFeatureLevel), ParticleBuffer, NormalsBuffer, PositionTexture, NormalTexture);

#pragma endregion


#pragma region ReadBackDebug

				FRDGBufferRef DebugBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("DebugSample"), sizeof(FVector3f), particleCount, nullptr, 0);

				FDebugPointsShaderInterface::AddPass_RenderThread(GraphBuilder, clothWidth, clothHeight, GetGlobalShaderMap(GMaxRHIFeatureLevel), DebugBuffer, PositionTexture);

				FRHIGPUBufferReadback* DebugReadback = new FRHIGPUBufferReadback(TEXT("DebugReadback"));
				AddEnqueueCopyPass(GraphBuilder, DebugReadback, DebugBuffer, sizeof(FVector3f) * particleCount);



				FRHIGPUBufferReadback* Readback = new FRHIGPUBufferReadback(TEXT("ClothReadback"));

				AddEnqueueCopyPass(
					GraphBuilder,
					Readback,
					ParticleBuffer,
					sizeof(FGPUParticle) * particles.Num()
				);

				GraphBuilder.Execute();

				AsyncTask(ENamedThreads::GameThread, [this, Readback, DebugReadback]()
					{
						this->PendingReadback = Readback;
						this->PendingDebugReadback = DebugReadback;
					});


#pragma endregion
			});
	}

	if (PendingReadback && PendingReadback->IsReady())
	{
		FRHIGPUBufferReadback* ReadbackToProcess = PendingReadback;
		PendingReadback = nullptr;

		int32 NumParticles = particles.Num();

		ENQUEUE_RENDER_COMMAND(ReadClothBack)(
			[this, ReadbackToProcess, NumParticles](FRHICommandListImmediate& RHICmdList)
			{
				const FGPUParticle* GPUData = (const FGPUParticle*)ReadbackToProcess->Lock(sizeof(FGPUParticle) * NumParticles);

				TArray<FVector3f> ResultPositions;
				ResultPositions.SetNumUninitialized(NumParticles);
				for (int32 i = 0; i < NumParticles; i++)
				{
					ResultPositions[i] = GPUData[i].position;
				}

				ReadbackToProcess->Unlock();
				delete ReadbackToProcess;

				AsyncTask(ENamedThreads::GameThread, [this, ResultPositions = MoveTemp(ResultPositions)]()
					{
						for (int32 i = 0; i < ResultPositions.Num() && i < particles.Num(); i++)
						{
							particles[i].position = ResultPositions[i];
						}

					});
			});
	}

	if (PendingDebugReadback && PendingDebugReadback->IsReady() && DebugParticleWPOPositions)
	{
		FRHIGPUBufferReadback* DebugReadbackToProcess = PendingDebugReadback;
		PendingDebugReadback = nullptr;

		int32 NumParticles = particles.Num();

		ENQUEUE_RENDER_COMMAND(ReadDebugBack)(
			[this, DebugReadbackToProcess, NumParticles](FRHICommandListImmediate& RHICmdList)
			{
				const FVector3f* DebugData = (const FVector3f*)DebugReadbackToProcess->Lock(sizeof(FVector3f) * NumParticles);

				TArray<FVector3f> DebugPositions;
				DebugPositions.SetNumUninitialized(NumParticles);
				for (int32 i = 0; i < NumParticles; i++)
				{
					DebugPositions[i] = DebugData[i];
				}

				DebugReadbackToProcess->Unlock();
				delete DebugReadbackToProcess;

				AsyncTask(ENamedThreads::GameThread, [this, DebugPositions = MoveTemp(DebugPositions)]()
					{
						LatestDebugPoints = DebugPositions;
					});
			});
	}

}

void AClothActor::RebuildCloth() {

}

void AClothActor::BuildParticles() {
	particles.SetNum(clothHeight * clothWidth);

	FTransform clothActorTransform = GetActorTransform();

	for (int y = 0; y < clothHeight; y++)
	{
		for (int x = 0; x < clothWidth; x++)
		{
			int i = y * clothWidth + x;

			FVector localPosition(x * particleSpacing, 0, -y * particleSpacing);
			FVector worldPosition = clothActorTransform.TransformPosition(localPosition);

			particles[i].position = FVector3f(worldPosition);

			particles[i].prevPosition = particles[i].position;

			particles[i].velocity = FVector3f::ZeroVector;

			particles[i].accumulatedForce = FVector3f::ZeroVector;

			particles[i].invMass = 1.0f;
		}
	}

	particles[0].invMass = 0;
	particles[clothWidth - 1].invMass = 0;


}

void AClothActor::BuildSprings() {
	springs.Empty();

	for (int y = 0; y < clothHeight; y++)
	{
		for (int x = 0; x < clothWidth; x++)
		{
			int i = y * clothWidth + x;
			//structural
			if (x < clothWidth - 1) {
				uint32 particleA = (uint32)i;
				uint32 particleB = (uint32)i + 1;
				float restLength = particleSpacing;
				float currentLambdaValue = 0.0f;

				FSpringCoordStore sData;
				sData.spring = { particleA, particleB, restLength, ClothCompilance, currentLambdaValue, ESpringType::VERTICAL };
				sData.xCoord = x;
				sData.yCoord = y;

				springs.Add(sData);
			}

			if (y < clothHeight - 1) {
				uint32 particleA = (uint32)i;
				uint32 particleB = (uint32)i + clothWidth;
				float restLength = particleSpacing;
				float currentLambdaValue = 0.0f;

				FSpringCoordStore sData;
				sData.spring = { particleA, particleB, restLength, ClothCompilance, currentLambdaValue, ESpringType::HORIZONTAL };
				sData.xCoord = x;
				sData.yCoord = y;

				springs.Add(sData);
			}

			//////shearing
			if (x < clothWidth - 1 && y < clothHeight - 1) {
				uint32 particleA = (uint32)i;
				uint32 particleB = (uint32)i + clothWidth + 1;
				float restLength = FMath::Sqrt(2.0f) * particleSpacing;
				float currentLambdaValue = 0.0f;
				float springCompliance = ClothCompilance * 0.7f;

				FSpringCoordStore sData;
				sData.spring = { particleA, particleB, restLength, springCompliance, currentLambdaValue, ESpringType::SHEARING_RIGHT };
				sData.xCoord = x;
				sData.yCoord = y;

				springs.Add(sData);
			}

			if (x > 0 && y < clothHeight - 1) {
				uint32 particleA = (uint32)i;
				uint32 particleB = (uint32)i + clothWidth - 1;
				float restLength = FMath::Sqrt(2.0f) * particleSpacing;
				float currentLambdaValue = 0.0f;
				float springCompliance = ClothCompilance * 0.7f;

				FSpringCoordStore sData;
				sData.spring = { particleA, particleB, restLength, springCompliance, currentLambdaValue, ESpringType::SHEARING_LEFT };
				sData.xCoord = x;
				sData.yCoord = y;

				springs.Add(sData);
			}

			//bending
			if (x < clothWidth - 2) {
				uint32 particleA = (uint32)i;
				uint32 particleB = (uint32)i + 2;
				float restLength = 2.0f * particleSpacing;
				float currentLambdaValue = 0.0f;
				float springCompliance = ClothCompilance * 0.5f;

				FSpringCoordStore sData;
				sData.spring = { particleA, particleB, restLength, springCompliance, currentLambdaValue, ESpringType::BENDING_RIGHT };
				sData.xCoord = x;
				sData.yCoord = y;

				springs.Add(sData);
			}

			if (y < clothHeight - 2) {
				uint32 particleA = (uint32)i;
				uint32 particleB = (uint32)i + 2 * clothWidth;
				float restLength = 2.0f * particleSpacing;
				float currentLambdaValue = 0.0f;
				float springCompliance = ClothCompilance * 0.5f;

				FSpringCoordStore sData;
				sData.spring = { particleA, particleB, restLength, springCompliance, currentLambdaValue, ESpringType::BENDING_LEFT };
				sData.xCoord = x;
				sData.yCoord = y;

				springs.Add(sData);
			}


		}
	}
}

static uint32_t AssignColour(int xCoord, int yCoord, ESpringType type) {
	switch (type) {
	case ESpringType::VERTICAL: return (uint32_t)(xCoord % 2);
	case ESpringType::HORIZONTAL: return (uint32_t)(yCoord % 2) + 2;
	case ESpringType::SHEARING_RIGHT: return (uint32_t)((xCoord % 2) + (yCoord % 2) * 2) + 4;
	case ESpringType::SHEARING_LEFT: return (uint32_t)((xCoord % 2) + (yCoord % 2) * 2) + 8;
	case ESpringType::BENDING_RIGHT: return (uint32_t)(((xCoord / 2) % 2) + (yCoord % 2) * 2) + 12;
	case ESpringType::BENDING_LEFT: return (uint32_t)((xCoord % 2) + ((yCoord / 2) % 2) * 2) + 16;
	default:
		return 0;
	}
}

void AClothActor::InitGPUSprings() {
	GPUSprings.Reserve(springs.Num());

	for (const FSpringCoordStore& tag : springs) {
		FGPUSpring spring;

		spring.particleA = tag.spring.particleA;
		spring.particleB = tag.spring.particleB;
		spring.restLength = tag.spring.restLength;
		spring.compliance = tag.spring.compliance;
		spring.lambda = 0.0f;

		spring.colour = AssignColour(tag.xCoord, tag.yCoord, tag.spring.type);

		GPUSprings.Add(spring);
	}

	GPUSprings.Sort([](const FGPUSpring& A, const FGPUSpring& B) {
		return A.colour < B.colour;
		});

	colourOffsets.SetNumZeroed(COLOURCOUNT);
	colourCount.SetNumZeroed(COLOURCOUNT);

	for (const FGPUSpring& spring : GPUSprings) {
		colourCount[spring.colour]++;
	}

	uint32 running = 0;
	for (int32 colour = 0; colour < COLOURCOUNT; colour++) {
		colourOffsets[colour] = running;
		running += colourCount[colour];
	}

}

void AClothActor::InitRendering() {
	PositionRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	PositionRenderTarget->RenderTargetFormat = RTF_RGBA32f; //using 32 here for better world pos precision
	PositionRenderTarget->bCanCreateUAV = true;
	PositionRenderTarget->InitAutoFormat(clothWidth, clothHeight);
	PositionRenderTarget->UpdateResourceImmediate(true);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, PositionRenderTarget, FLinearColor(0, 0, 0, 0));

	NormalsRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	NormalsRenderTarget->RenderTargetFormat = RTF_RGBA16f; // normals only need -1 to 1 range
	NormalsRenderTarget->bCanCreateUAV = true;
	NormalsRenderTarget->InitAutoFormat(clothWidth, clothHeight);
	NormalsRenderTarget->UpdateResourceImmediate(true);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, NormalsRenderTarget, FLinearColor(0, 1, 0, 0));

	if (ClothMeshComp && ClothMaterial) {
		ClothDynamicMat = UMaterialInstanceDynamic::Create(ClothMaterial, this);
		ClothDynamicMat->SetTextureParameterValue(TEXT("PositionMap"), PositionRenderTarget);
		UTexture* Tex = nullptr;
		ClothDynamicMat->GetTextureParameterValue(
			FMaterialParameterInfo(TEXT("PositionMap")),
			Tex);

		UE_LOG(LogTemp, Warning,
			TEXT("Bound texture: %s"),
			Tex ? *Tex->GetName() : TEXT("NULL"));
		ClothDynamicMat->SetTextureParameterValue(TEXT("NormalMap"), NormalsRenderTarget);
		ClothMeshComp->SetMaterial(0, ClothDynamicMat);
	}
}

void AClothActor::BuildClothMesh() {
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colours;
	TArray<int32> Tris;
	TArray<FProcMeshTangent> Tangents;

	Vertices.SetNum(clothWidth * clothHeight);
	Normals.SetNum(clothWidth * clothHeight);
	UVs.SetNum(clothWidth * clothHeight);

	uint32 h = (uint32)clothHeight;
	uint32 w = (uint32)clothWidth;

	for (uint32 y = 0; y < h; y++) {
		for (uint32 x = 0; x < w; x++) {
			uint32 idx = y * w + x;
			Vertices[idx] = FVector(x * particleSpacing, 0, -(float)y * particleSpacing);
			Normals[idx] = FVector::UpVector;
			UVs[idx] = FVector2D((float)x / (w - 1), (float)y / (h - 1));
		}
	}


	for (uint32 y = 0; y < (h - 1); y++) {
		for (uint32 x = 0; x < (w - 1); x++) {
			uint32 i0 = y * w + x;
			uint32 i1 = i0 + 1;
			uint32 i2 = i0 + w;
			uint32 i3 = i2 + 1;

			Tris.Add(i0);
			Tris.Add(i2);
			Tris.Add(i1);

			Tris.Add(i1);
			Tris.Add(i2);
			Tris.Add(i3);
		}
	}

	ClothMeshComp->CreateMeshSection_LinearColor(0, Vertices, Tris, Normals, UVs, Colours, Tangents, false);
}