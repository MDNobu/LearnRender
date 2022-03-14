// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeformMeshActor.generated.h"

class UDeformMeshComponent;

UCLASS()
class CUSTOMSHADERMODULE_API ADeformMeshActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADeformMeshActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "DeformMesh")
	UDeformMeshComponent* DeformMeshComp;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "DeformMesh")
	UStaticMesh* SourceMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "DeformMesh")
	AActor* DeformController;
};
