// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RHIGPUReadback.h"

#include "ClothTest.generated.h"

#define COLOURCOUNT 20

UENUM(BlueprintType)
enum class ESpringType : uint8 {
	VERTICAL,
	HORIZONTAL,
	SHEARING,
	SHEARING_LEFT,
	SHEARING_RIGHT,
	BENDING_LEFT,
	BENDING_RIGHT,
	BENDING,
};

USTRUCT()
struct FClothParticle {
	GENERATED_BODY()

	UPROPERTY()
	FVector3f prevPosition = FVector3f::ZeroVector;

	UPROPERTY()
	FVector3f Position = FVector3f::ZeroVector;

	UPROPERTY()
	FVector3f accumulatedForce = FVector3f::ZeroVector;

	UPROPERTY()
	FVector3f velocity = FVector3f::ZeroVector;

	UPROPERTY()
	float invMass = 0.0f;
};

USTRUCT()
struct FGPUParticle {
	GENERATED_BODY()

	FVector3f position;
	float _pad1;
	FVector3f prevPosition;
	float _pad2;
	FVector3f velocity;
	float _pad3;
	FVector3f accumulatedForce;
	float _pad4;
	float invMass;
	FVector3f prevCollisionNormal;
};

USTRUCT()
struct FSpring {
	GENERATED_BODY()

	FSpring() = default;

	FSpring(
		uint32 InParticleA,
		uint32 InParticleB,
		float InRestLength,
		float InCompliance,
		float InLambda,
		ESpringType InType,
		uint32 InColour = 0)
		: particleA(InParticleA)
		, particleB(InParticleB)
		, restLength(InRestLength)
		, compliance(InCompliance)
		, lambda(InLambda)
		, type(InType)
		, colour(InColour)
	{
	}

	UPROPERTY()
	uint32 particleA = 0;

	UPROPERTY()
	uint32 particleB = 0;

	UPROPERTY()
	float restLength = 0.0f;

	UPROPERTY()
	float compliance = 0.0f;

	UPROPERTY()
	float lambda = 0.0f;

	UPROPERTY()
	ESpringType type = ESpringType::VERTICAL;

	UPROPERTY()
	uint32 colour = 0;
};

USTRUCT()
struct FSpringCoordStore {
	GENERATED_BODY()

	FSpring spring;
	int xCoord, yCoord;
};

USTRUCT()
struct FGPUSpring {
	GENERATED_BODY()

	UPROPERTY()
	uint32 particleA;

	UPROPERTY()
	uint32 particleB;

	UPROPERTY()
	float restLength;

	UPROPERTY()
	float compliance;

	UPROPERTY()
	float lambda;

	UPROPERTY()
	uint32 colour;

	uint32 pad[2];
};

UCLASS()
class TESTING_API AClothTest : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AClothTest();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cloth|Physics")
	FVector3f Gravity = FVector3f(0.0, 0.0, -981.0);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void BuildParticles();

	void BuildSprings();

	void InitGPUSprings();

	UPROPERTY()
	TArray<FGPUParticle> particles;

	UPROPERTY()
	TArray<FSpringCoordStore> springs;

	UPROPERTY()
	TArray<FGPUSpring> GPUSprings;


	UPROPERTY()
	TArray<uint32> colourOffsets;

	UPROPERTY()
	TArray<uint32> colourCount;

	UPROPERTY(EditAnywhere)
	int32 clothWidth = 10;

	UPROPERTY(EditAnywhere)
	int32 clothHeight = 10;

	UPROPERTY(EditAnywhere)
	float particleSpacing = 10.0f;

	UPROPERTY(EditAnywhere)
	int32 numSubsteps = 8;

	UPROPERTY(EditAnywhere)
	int32 numIterations = 2;

	UPROPERTY(EditAnywhere)
	float ClothCompilance = 0.0005f;

	FRHIGPUBufferReadback* PendingReadback = nullptr;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
