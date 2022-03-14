// Fill out your copyright notice in the Description page of Project Settings.


#include "DeformMeshActor.h"
#include "DeformMeshComponent.h"

// Sets default values
ADeformMeshActor::ADeformMeshActor()
{
	DeformMeshComp =  CreateDefaultSubobject< UDeformMeshComponent>(TEXT("DeformComponnet"));
	DeformController = CreateDefaultSubobject<AActor>(TEXT("DeformController"));

 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ADeformMeshActor::BeginPlay()
{
	Super::BeginPlay();
	
	DeformMeshComp->CreateSection(0, SourceMesh, DeformController->GetTransform());
}

// Called every frame
void ADeformMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DeformMeshComp->UpdateSection(0, DeformController->GetTransform());
	DeformMeshComp->FinishDeformUpdate();
}

