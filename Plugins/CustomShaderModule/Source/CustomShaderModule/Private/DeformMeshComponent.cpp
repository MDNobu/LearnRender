#include "DeformMeshComponent.h"
#include "MeshMaterialShader.h"
#include "LocalVertexFactory.h"


struct FDeformMeshVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FDeformMeshVertexFactory);
public:
	FDeformMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FDeformMeshVertexFactory")
	{
		// 不需要manual vertex fetch
		bSupportsManualVertexFetch = false;
	}

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
	{
		if ( (Parameters.MaterialParameters.MaterialDomain == MD_Surface && 
			Parameters.MaterialParameters.ShadingModels == MSM_Unlit) || 
			(Parameters.MaterialParameters.bIsDefaultMaterial))
		{
			return true;
		}
		return false;
	}

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		const bool containsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH");
		if (!containsManualVertexFetch)
		{
			OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
		}

		OutEnvironment.SetDefine(TEXT("DEFORM_MESH"), TEXT("1"));
	}



	/**
	 * 这时资源初始化的主要方法， 
	 * 在这里可以设置vertex stream 和vertex declaration
	 * 在父类LocalVertexFactory中，3个vertex declaration被初始化： PostionOnly, PositionAndNormalOnly, 和default
	 * default be used in main rendering
	 * 
	 */
	void InitRHI() override
	{
		
	}

};

class FDeformMeshSectionProxy
{
public:
	FDeformMeshSectionProxy(ERHIFeatureLevel::Type InFeatureLevel)
		Material(nullptr),
	{

	}

	// 当前section 的材质
	UMaterialInstance* Material;

	FRawStaticIndexBuffer IndexBuffer;

	FDeformMeshVertexFactory VertexFactory;


};

class FDeformMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FDeformMeshSceneProxy(UDeformMeshComponent* deformCom)
		: FPrimitiveSceneProxy(deformCom)
		, MaterialRelevance(deformCom->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		const uint16 numSections = deformCom->DeformMeshSections.Num();

		DeformTransforms.AddZeroed(numSections); // #Unkown 什么时候用 为什么不用reserve
		Sections.AddZeroed(numSections); 

#pragma region CreateSectionProxies
		for (uint16 sectionIndex = 0; sectionIndex < numSections; sectionIndex++)
		{
			const FDeformMeshSection& srcSection = deformCom->DeformMeshSections[sectionIndex];
			// 创建和初始化section proxy
			{
				FDeformMeshSectionProxy* newSectionProxy = new FDeformMeshSectionProxy(GetScene().GetFeatureLevel());
				 //srcSection.StaticMesh->RenderData->LODResources[0]


				Sections[sectionIndex] = newSectionProxy;
			}
		}
#pragma endregion


	}


	void UpdateDeformTransform_RenderThread(int32 SectionIndex, FMatrix DeformTransform)
	{
		check(IsInActualRenderingThread());
		/*if (SectionIndex < )
		{

		}*/
	}

	uint32 GetAllocatedSize(void) const
	{
		return (FPrimitiveSceneProxy::GetAllocatedSize());
	}

	inline FShaderResourceViewRHIRef& GetDeformTransformsSRV() { return DeformTransformSRV;}
private:
	TArray<FDeformMeshSectionProxy*> Sections;
	

	FMaterialRelevance MaterialRelevance;
	
	//  每个section 有不同的deform transform
	TArray<FMatrix> DeformTransforms;

	// 包括deform transform信息， 会作为shader resource 传入 shader
	FStructuredBufferRHIRef DeformTransformsSB;

	// structed buffer的shader resource view, 将会绑定到vertex factory 
	FShaderResourceViewRHIRef DeformTransformSRV;

	// structed buffers是否需要更新的dirty flag
	bool bDeformTransformsDirty;
};


void UDeformMeshComponent::CreateSection(int32 SectionIndex, 
	UStaticMesh* SourceMesh, const FTransform& DeformTransform)
{
	if (SectionIndex >= DeformMeshSections.Num())
	{
		DeformMeshSections.SetNum(SectionIndex + 1, false);
	}
	
	FDeformMeshSection& newSection = DeformMeshSections[SectionIndex];
	newSection.Reset();

	newSection.StaticMesh = SourceMesh;
	newSection.DeformTransform = DeformTransform.ToMatrixWithScale().GetTransposed(); //#Unkown 不懂

	newSection.StaticMesh->CalculateExtendedBounds();
	newSection.SectionBoundingBox += newSection.StaticMesh->GetBoundingBox();

	SetMaterial(SectionIndex, newSection.StaticMesh->GetMaterial(0));

	// 更新bounds
	UpdateLocalBounds();

	// Need to send to render thread
	MarkRenderTransformDirty();
}

void UDeformMeshComponent::UpdateSection(int32 SectionIndex, const FTransform& DeformTransform)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		const FMatrix transformMatrix = DeformTransform.ToMatrixWithScale().GetTransposed();
		DeformMeshSections[SectionIndex].DeformTransform = transformMatrix;

		DeformMeshSections[SectionIndex].SectionBoundingBox += 
			DeformMeshSections[SectionIndex].StaticMesh->GetBoundingBox().TransformBy(DeformTransform);

		if (SceneProxy)
		{
			FDeformMeshSceneProxy* deformMeshSceneProxy = (FDeformMeshSceneProxy*)SceneProxy;
			
			ENQUEUE_RENDER_COMMAND(FDeformTransformUpdate)(
				[deformMeshSceneProxy, SectionIndex, transformMatrix](FRHICommandListImmediate& RHICmdList)
				{
					deformMeshSceneProxy->UpdateDeformTransform_RenderThread(SectionIndex, transformMatrix);
				}
				);
		}

		UpdateLocalBounds();
		MarkRenderTransformDirty();
	}
}

void UDeformMeshComponent::FinishDeformUpdate()
{
	
}

void UDeformMeshComponent::ClearSection(int32 SectionIndex)
{
	
}

void UDeformMeshComponent::ClearAllMeshSections()
{

}

void UDeformMeshComponent::SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility)
{

}

void UDeformMeshComponent::UpdateLocalBounds()
{
	FBox localBox(ForceInit);

	for (const FDeformMeshSection& deformSection: DeformMeshSections)
	{
		localBox += deformSection.SectionBoundingBox;
	}

	LocalBounds = localBox.IsValid ? FBoxSphereBounds(localBox) : 
		FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fall back 到reset

	UpdateBounds();

	MarkRenderTransformDirty();
}
