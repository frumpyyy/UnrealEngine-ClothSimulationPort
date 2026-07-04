#include "ClothTest.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "RHIGPUReadback.h"

#include "Shader_XPBDCloth.h"
#include "Shader_Integrate.h"
#include "Shader_ResetLambda.h"
#include "Shader_ApplyForce.h"

#define NUMSUBSTEPS 1

// Sets default values
AClothTest::AClothTest()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AClothTest::BeginPlay()
{
	Super::BeginPlay();

	BuildParticles();
	BuildSprings();
	InitGPUSprings();

	UE_LOG(LogTemp, Warning, TEXT("Particles: %d"), particles.Num());
	UE_LOG(LogTemp, Warning, TEXT("Springs: %d"), springs.Num());

}

// Called every frame
void AClothTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!PendingReadback)
	{
		ENQUEUE_RENDER_COMMAND(RunCloth)(
			[this, DeltaTime](FRHICommandListImmediate& RHIcmdList)
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

#pragma endregion

#pragma region RenderPasses

				for (int i = 0; i < NUMSUBSTEPS; i++) {

					FResetLambdaShaderInterface::AddPass_RenderThread(GraphBuilder, springs.Num(), GetGlobalShaderMap(GMaxRHIFeatureLevel), SpringBuffer);

					FApplyForceShaderInterface::AddPass_RenderThread(GraphBuilder, particles.Num(), { 0,0,-981.0f }, GetGlobalShaderMap(GMaxRHIFeatureLevel), ParticleBuffer);

					FIntegrateShaderInterface::AddPass_RenderThread(GraphBuilder, particles.Num(), NUMSUBSTEPS, DeltaTime, GetGlobalShaderMap(GMaxRHIFeatureLevel),
						ParticleBuffer);


					for (int32 c = 0; c < COLOURCOUNT; c++)
					{
						if (colourCount[c] == 0)
						{
							continue;
						}

						FSolveSpringsShaderInterface::AddPass_RenderThread(GraphBuilder, colourOffsets[c], colourCount[c], NUMSUBSTEPS, DeltaTime, GetGlobalShaderMap(GMaxRHIFeatureLevel),
							ParticleBuffer, SpringBuffer);
					}
				}




#pragma endregion

#pragma region ReadBackDebug

				FRHIGPUBufferReadback* Readback = new FRHIGPUBufferReadback(TEXT("ClothReadback"));

				AddEnqueueCopyPass(
					GraphBuilder,
					Readback,
					ParticleBuffer,
					sizeof(FGPUParticle) * particles.Num()
				);

				GraphBuilder.Execute();

				AsyncTask(ENamedThreads::GameThread, [this, Readback]()
					{
						this->PendingReadback = Readback;
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

						UE_LOG(LogTemp, Warning, TEXT("P0: %s  P1: %s"),
							*particles[0].position.ToString(), *particles[1].position.ToString());
					});
			});
	}

	for (const FGPUParticle& P : particles)
	{
		DrawDebugPoint(GetWorld(), (FVector)P.position, 8.f, FColor::Red, false, -1.f, 0);
	}

}

void AClothTest::BuildParticles() {
	particles.SetNum(clothHeight * clothWidth);
	for (int y = 0; y < clothHeight; y++)
	{
		for (int x = 0; x < clothWidth; x++)
		{
			int i = y * clothWidth + x;

			particles[i].position = FVector3f(x * particleSpacing, 0, y * particleSpacing);

			particles[i].prevPosition = particles[i].position;

			particles[i].velocity = FVector3f::ZeroVector;

			particles[i].accumulatedForce = FVector3f::ZeroVector;

			particles[i].invMass = 1.0f;
		}
	}

	particles[0].invMass = 0;
	particles[clothWidth - 1].invMass = 0;


}

void AClothTest::BuildSprings() {
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

void AClothTest::InitGPUSprings() {
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