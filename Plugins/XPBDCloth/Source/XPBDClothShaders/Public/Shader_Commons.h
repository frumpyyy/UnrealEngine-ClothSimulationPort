

#pragma once

#include "CoreMinimal.h"

struct Particle
{
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

struct Spring
{
	UINT A, B;
	float restingLength, lambda, compliance;
	UINT colour;
	UINT pad[2];
};
