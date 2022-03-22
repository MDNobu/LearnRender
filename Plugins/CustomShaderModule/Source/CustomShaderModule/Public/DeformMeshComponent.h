// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "DeformMeshComponent.generated.h"


USTRUCT()
struct FDeformMeshSection
{
	GENERATED_BODY()
public:
	
	// 这个section 的static mesh
	UPROPERTY()
	UStaticMesh* StaticMesh;

	UPROPERTY()
	FMatrix DeformTransform;

	UPROPERTY()
	FBox SectionBoundingBox;

	UPROPERTY()
	bool bSectionVisible;

	FDeformMeshSection()
		: SectionBoundingBox(ForceInit) // 初始化FBox为无效
		, bSectionVisible(true)
	{
		
	}

	void Reset()
	{
		StaticMesh = nullptr;
		SectionBoundingBox.Init();
		bSectionVisible = true;
	}
};

/**
 * 
 */
UCLASS()
class CUSTOMSHADERMODULE_API UDeformMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
	
public:
	void CreateSection(int32 SectionIndex, UStaticMesh* SourceMesh, const FTransform& DeformTransform);

	void UpdateSectionTransform(int32 SectionIndex, const FTransform& DeformTransform);

	void FinishDeformUpdate();

	void ClearSection(int32 SectionIndex);

	void ClearAllMeshSections();

	void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);



	FPrimitiveSceneProxy* CreateSceneProxy() override;


	int32 GetNumMaterials() const override;


	FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

public:

	//FPrimitiveSceneProxy* SceneProxy;


private:
	UPROPERTY()
	TArray<FDeformMeshSection> DeformMeshSections;

	UPROPERTY()
	FBoxSphereBounds LocalBounds;

	friend class FDeformMeshSceneProxy;
	void UpdateLocalBounds();
};
