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
		// ����Ҫmanual vertex fetch
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
	 * ��ʱ��Դ��ʼ������Ҫ������ 
	 * �������������vertex stream ��vertex declaration
	 * �ڸ���LocalVertexFactory�У�3��vertex declaration����ʼ���� PostionOnly, PositionAndNormalOnly, ��default
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

	// ��ǰsection �Ĳ���
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

		DeformTransforms.AddZeroed(numSections); // #Unkown ʲôʱ���� Ϊʲô����reserve
		Sections.AddZeroed(numSections); 

#pragma region CreateSectionProxies
		for (uint16 sectionIndex = 0; sectionIndex < numSections; sectionIndex++)
		{
			const FDeformMeshSection& srcSection = deformCom->DeformMeshSections[sectionIndex];
			// �����ͳ�ʼ��section proxy
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
	
	//  ÿ��section �в�ͬ��deform transform
	TArray<FMatrix> DeformTransforms;

	// ����deform transform��Ϣ�� ����Ϊshader resource ���� shader
	FStructuredBufferRHIRef DeformTransformsSB;

	// structed buffer��shader resource view, ����󶨵�vertex factory 
	FShaderResourceViewRHIRef DeformTransformSRV;

	// structed buffers�Ƿ���Ҫ���µ�dirty flag
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
	newSection.DeformTransform = DeformTransform.ToMatrixWithScale().GetTransposed(); //#Unkown ����

	newSection.StaticMesh->CalculateExtendedBounds();
	newSection.SectionBoundingBox += newSection.StaticMesh->GetBoundingBox();

	SetMaterial(SectionIndex, newSection.StaticMesh->GetMaterial(0));

	// ����bounds
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
		FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 0); // fall back ��reset

	UpdateBounds();

	MarkRenderTransformDirty();
}
