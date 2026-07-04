#pragma once

#include "CoreMinimal.h"

struct FGPUP {
	FVector3f position; float _pad1;
	FVector3f prevPosition; float _pad2;
	FVector3f velocity; float _pad3;
	FVector3f accumulatedForce; float _pad4;
	float invMass;
	FVector3f prevCollisionNormal;
};

struct FGPUS {
	uint32 particleA;

	uint32 particleB;

	float restLength;

	float compliance;

	float lambda;

	uint32 colour;
};


class XPBDCLOTHSHADERS_API FSolveSpringsShaderInterface {
public:
	static void AddPass_RenderThread(FRDGBuilder& graphBuilder, uint32 inSpringOffset, uint32 inSpringCount, uint32 inNumSubsteps, float inDt, uint32 groupSize, FGlobalShaderMap* shaderMap, FRDGBufferRef particlesBufferRef, FRDGBufferRef springsBufferRef);
};