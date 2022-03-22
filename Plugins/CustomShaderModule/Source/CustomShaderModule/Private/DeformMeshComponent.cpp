#include "DeformMeshComponent.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshResources.h"
#include "MeshMaterialShader.h"
#include "ShaderParameters.h"
#include "RHIUtilities.h"

#include "MeshMaterialShader.h"
#include "MeshDrawShaderBindings.h"
#include "MeshBatch.h"
#include "RHIDefinitions.h"
#include "SceneView.h"
#include "SceneInterface.h"


class FDeformMeshSceneProxy;
class FDeformMeshSectionProxy;
class FDeformMeshVertexFactoryShaderParameters;
struct FDeformMeshVertexFactory;

static void InitVertexFactoryData(FDeformMeshVertexFactory* VertexFactory,
	FStaticMeshVertexBuffers* VertexBuffer);

/**
 * Defrom Mesh Component 的vertex factory
 * 
 */
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

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory?
	 * 这个方法可以自定义那些shader可以编译,
	 * Default material一般要支持，否则可能crash
	 */
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

	/**
	 * 这个可以开关shader中的预定义宏
	 */
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
	 * 在父类LocalVertexFactory中，3个vertex declaration被初始化：
	 * PostionOnly, PositionAndNormalOnly, 和default
	 * default be used in main rendering
	 * 
	 */
	void InitRHI() override
	{
		check(HasValidFeatureLevel());

		FVertexDeclarationElementList elements;  // used for default vertex stream
		FVertexDeclarationElementList posOnlyElements; // position only vertex stream用

		if (Data.PositionComponent.VertexBuffer != NULL)
		{
			//用AccessStreamComponent 从 vertexStreamComponent获得一个vertexElement
			elements.Add(AccessStreamComponent(Data.PositionComponent, 0));  
			posOnlyElements.Add(AccessStreamComponent(Data.PositionComponent, 0));
		}

		// 初始化position only declaration，这个是在depth pass中用的 
		// 间接调用PipelineStateCache::GetOrCreateVertexDeclartion 返回 RHId的declartion
		InitDeclaration(posOnlyElements, EVertexInputStreamType::PositionOnly);

		// 添加所有texcoords to default elements,对于unlit shading 这足够了
		if (Data.TextureCoordinates.Num())
		{
			const int32 baseTexCoorAttribute = 4;  //这里为啥不是从0开始 #Unkown
			for (int32 coordinateIndex = 0; coordinateIndex < Data.TextureCoordinates.Num();
				coordinateIndex++)
			{
				elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[coordinateIndex],
					baseTexCoorAttribute + coordinateIndex));
			}

			// 下面这里是为啥？？？？
			for (int32 coordinateIndex = Data.TextureCoordinates.Num(); coordinateIndex < MAX_STATIC_TEXCOORDS/2; coordinateIndex++)
			{
				elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
					baseTexCoorAttribute + coordinateIndex));
			}

		}

		check(Streams.Num() > 0);

		InitDeclaration(elements);
		check(IsValidRef(GetDeclaration()));
	}


	inline void SetTransformIndex(uint16 val) { TransformIndex = val; }
	inline void SetSceneProxy(FDeformMeshSceneProxy* val) { SceneProxy = val; }
private:

	// 这个用来shader中用这个获取transform 
	uint16 TransformIndex;

	// 这个用来获取component prxy的 unified shader resource view
	FDeformMeshSceneProxy* SceneProxy;

	friend class FDeformMeshVertexFactoryShaderParameters;
};




class FDeformMeshSectionProxy
{
public:
	FDeformMeshSectionProxy(ERHIFeatureLevel::Type InFeatureLevel)
		: SectionMaterial(nullptr)
		, VertexFactory(InFeatureLevel)
		, bSectionVisible(true)
	{
	}

	// 当前section 的材质
	UMaterialInterface* SectionMaterial = nullptr;

	FRawStaticIndexBuffer IndexBuffer;

	FDeformMeshVertexFactory VertexFactory;

	bool bSectionVisible;
	/* Max vertix index is an info that 
	* is needed when rendering the mesh, so we 
	* cache it here so we don't have to pointer chase it later
	*/
	uint32 MaxVertexIndex;
};


class FDeformMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FDeformMeshSceneProxy(UDeformMeshComponent* deformCom)
		: FPrimitiveSceneProxy(deformCom)
		, MaterialRelevance(deformCom->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		const uint16 numSections = deformCom->DeformMeshSections.Num();

		DeformTransforms.AddZeroed(numSections);
		Sections.AddZeroed(numSections);

		Sections.Reserve(numSections);
#pragma region CreateSectionProxies
		for (uint16 sectionIndex = 0; sectionIndex < numSections; sectionIndex++)
		{
			const FDeformMeshSection& srcSection = deformCom->DeformMeshSections[sectionIndex];
			// 创建和初始化section proxy
			{
				FDeformMeshSectionProxy* newSectionProxy = new FDeformMeshSectionProxy(
					GetScene().GetFeatureLevel());
				//srcSection.StaticMesh->RenderData->LODResources[0]

				auto& LODResource = srcSection.StaticMesh->RenderData->LODResources[0];

				FDeformMeshVertexFactory* vertexFactory = &newSectionProxy->VertexFactory;

				// 用static mesh中的值初始化vertex factory
				InitVertexFactoryData(vertexFactory, &(LODResource.VertexBuffers));

				vertexFactory->SetTransformIndex(sectionIndex);
				vertexFactory->SetSceneProxy(this);

				// 复制static mesh的index buffer，使用这个mesh section的index buffer
				{
					TArray<uint32> tmp_indices;
					LODResource.IndexBuffer.GetCopy(tmp_indices);
					newSectionProxy->IndexBuffer.AppendIndices(tmp_indices.GetData(),
						tmp_indices.Num());
					// 
					BeginInitResource(&newSectionProxy->IndexBuffer);
				}


				DeformTransforms[sectionIndex] = srcSection.DeformTransform;

				// 设置section 的maxVertexIndex
				newSectionProxy->MaxVertexIndex =
					LODResource.VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

				newSectionProxy->SectionMaterial = deformCom->GetMaterial(sectionIndex);

				if (newSectionProxy->SectionMaterial == nullptr)
				{
					newSectionProxy->SectionMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				newSectionProxy->bSectionVisible = srcSection.bSectionVisible;

				Sections[sectionIndex] = newSectionProxy;
			}
		}
#pragma endregion

#pragma region CreateStructedBufferForSections
		if (numSections > 0)
		{
			// 创建structed buffer for所有section的deform transform
			// 当前一个component的所有sections用一个structed buffer, true表示需要CPU访问
			TResourceArray<FMatrix>* resourceArray = new TResourceArray<FMatrix>(true);

			FRHIResourceCreateInfo createInfo;
			resourceArray->Append(DeformTransforms);
			createInfo.ResourceArray = resourceArray;
			// 这里设置的DebugName是为了能够在RenderDoc中找到
			createInfo.DebugName = TEXT("DeformMesh_TransformsSB");

			DeformTransformsSB = RHICreateStructuredBuffer(sizeof(FMatrix),
				numSections * sizeof(FMatrix), BUF_ShaderResource, createInfo);
			bDeformTransformsDirty = false;

			// 为structed buffer创建shader resource view
			DeformTransformSRV = RHICreateShaderResourceView(DeformTransformsSB);
		}
#pragma endregion

	}

	virtual ~FDeformMeshSceneProxy()
	{
		// 释放每个section 的render resource
		for (FDeformMeshSectionProxy* section : Sections)
		{
			if (section != nullptr)
			{
				section->IndexBuffer.ReleaseResource();
				section->IndexBuffer.ReleaseResource();
				delete section;
			}
		}

		// 释放structed buffer和其srv
		DeformTransformsSB.SafeRelease();
		DeformTransformSRV.SafeRelease();

	}

	void UpdateDeformTransformSB_RenderThread()
	{
		check(IsInActualRenderingThread());
		if (bDeformTransformsDirty && DeformTransformsSB)
		{
			void* sbData = RHILockStructuredBuffer(DeformTransformsSB, 0,
				DeformTransforms.Num() * sizeof(FMatrix), RLM_WriteOnly);
			FMemory::Memcpy(sbData, DeformTransforms.GetData(),
				DeformTransforms.Num() * sizeof(FMatrix));
			RHIUnlockStructuredBuffer(DeformTransformsSB);
			bDeformTransformsDirty = false;
		}
	}

	/**
	 * 更新 deformTransform structed buffer
	 */
	void UpdateDeformTransformSB_RenderThread(int32 SectionIndex, FMatrix DeformTransform)
	{
		check(IsInActualRenderingThread());
		// 根据需要更新struted buffer
		if (bDeformTransformsDirty && DeformTransformsSB)
		{
			void* bufferData = RHILockStructuredBuffer(
				DeformTransformsSB,
				0, // offset
				DeformTransforms.Num() * sizeof(FMatrix), // buffer size
				RLM_WriteOnly
			);
			FMemory::Memcpy(bufferData, DeformTransforms.GetData(),
				DeformTransforms.Num() * sizeof(FMatrix));
			RHIUnlockStructuredBuffer(DeformTransformsSB);
			bDeformTransformsDirty = false;
		}
	}

	/**
	 * 更新 对应section id 的transform ，只需要更新section array 里的值，注意mark dirty
	 */
	void UpateDeformTransofm_RenderThread(int32 SectionIndex, FMatrix DeformTransform)
	{
		check(IsInRenderingThread());

		if (SectionIndex < Sections.Num() &&
			Sections[SectionIndex] != nullptr)
		{
			DeformTransforms[SectionIndex] = DeformTransform;
			bDeformTransformsDirty = true;
		}
	}

	void SetSectionVisibility_RenderThread(int32 SectionIndex, bool bNewVisibility)
	{
		check(IsInActualRenderingThread());

		if (SectionIndex < Sections.Num() &&
			Sections[SectionIndex] != nullptr)
		{
			Sections[SectionIndex]->bSectionVisible = bNewVisibility;

			//#Unkown 这里不需要mark dirty 吗?????????
			//bDeformTransformsDirty = true;
		}
	}

	/**
	 * 根据views和visiblityMap,我们需要构建MeshBatch
	 */
	void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
		class FMeshElementCollector& Collector) const override
	{
		const bool bUseWireframe = AllowDebugViewmodes() &&
			ViewFamily.EngineShowFlags.Wireframe;

		FColoredMaterialRenderProxy* wireframeMaterialInstance = nullptr;
		if (bUseWireframe)
		{
			wireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,
				FLinearColor(0.f, 0.5f, 1.f)
			);

			// 将proxy的内存回收工作交给collector
			Collector.RegisterOneFrameMaterialProxy(wireframeMaterialInstance);
		}

		for (const FDeformMeshSectionProxy* sectionProxy : Sections)
		{
			if (sectionProxy && sectionProxy->bSectionVisible)
			{
				FMaterialRenderProxy* materialProxy = bUseWireframe ?
					wireframeMaterialInstance : sectionProxy->SectionMaterial->GetRenderProxy();

				// foreach view
				for (int32 viewIndex = 0; viewIndex < Views.Num(); viewIndex++)
				{
					// 检查mesh对当前view是否可见
					bool bVisibleToView = VisibilityMap & (1 << viewIndex);
					if (bVisibleToView)
					{
						const FSceneView* sceneView = Views[viewIndex];

#pragma region ContructMeshBatch
						FMeshBatch& meshBatch = Collector.AllocateMesh();
						FMeshBatchElement& batchElement = meshBatch.Elements[0];
#pragma endregion
						batchElement.IndexBuffer = &sectionProxy->IndexBuffer;
						meshBatch.bWireframe = bUseWireframe;
						meshBatch.VertexFactory = &sectionProxy->VertexFactory;
						meshBatch.MaterialRenderProxy = materialProxy;

						// LocalVertexFactory 用一个uniform buffer 来
						// 传递localToWorld previousLocalToWorld等等, 大部分都能用下面的helper function完成
						bool bHasPrecomputedVolumetricLightmap;
						FMatrix prevousLocalToWorld;
						int32 singleCaptureIndex;
						bool bOutputVelocity;
						GetScene().GetPrimitiveUniformShaderParameters_RenderThread(
							GetPrimitiveSceneInfo(),
							bHasPrecomputedVolumetricLightmap,
							prevousLocalToWorld,
							singleCaptureIndex,
							bOutputVelocity
						);
						FDynamicPrimitiveUniformBuffer& dynamicUniformBuffer =
							Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						// 分配一个临时primitive uniform buffer, 填充它并设置到batchElement中
						dynamicUniformBuffer.Set(GetLocalToWorld(),
							prevousLocalToWorld,
							GetBounds(), GetLocalBounds(),
							true, bHasPrecomputedVolumetricLightmap,
							DrawsVelocity(), bOutputVelocity);
						batchElement.PrimitiveUniformBufferResource = &dynamicUniformBuffer.UniformBuffer;
						batchElement.PrimitiveIdMode = PrimID_DynamicPrimitiveShaderData;

						// additional data
						batchElement.FirstIndex = 0;
						batchElement.NumPrimitives = sectionProxy->IndexBuffer.GetNumIndices() / 3;
						batchElement.MinVertexIndex = 0;
						batchElement.MaxVertexIndex = sectionProxy->MaxVertexIndex;

						meshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
						meshBatch.Type = PT_TriangleList;
						meshBatch.DepthPriorityGroup = SDPG_World;
						meshBatch.bCanApplyViewModeOverrides = false;

						//add batch to collector
						Collector.AddMesh(viewIndex, meshBatch);
					}
				}
			}
		}
	}

	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance res;
		res.bDrawRelevance = IsShown(View);
		res.bShadowRelevance = IsShadowCast(View);
		res.bDynamicRelevance = true;
		res.bRenderInMainPass = ShouldRenderInMainPass();
		res.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		res.bRenderCustomDepth = ShouldRenderCustomDepth();
		res.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		// cache这个在can be occluded中用
		MaterialRelevance.SetPrimitiveViewRelevance(res);
		res.bVelocityRelevance = IsMovable() && res.bOpaque && res.bRenderInMainPass;
		return res;
	}

	bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}


	uint32 GetAllocatedSize(void) const
	{
		return (FPrimitiveSceneProxy::GetAllocatedSize());
	}

	inline FShaderResourceViewRHIRef& GetDeformTransformsSRV() { return DeformTransformSRV; }

	virtual uint32 GetMemoryFootprint(void) const override
	{
		return (sizeof(*this) + GetAllocatedSize());
	}


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

//#Unkown 啥是 Mannual fetch
class FDeformMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	// #Unkown 这个是干啥的
	DECLARE_TYPE_LAYOUT(FDeformMeshVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		TransformIndex.Bind(ParameterMap, TEXT("DMTransformIndex"), SPF_Optional);
		TransformSRV.Bind(ParameterMap, TEXT("DMTransforms"), SPF_Optional);
	}

	/**
	 * 这个是在构建 mesh draw commmand 的的时候 会调用到，
	 * 需要输出shaderBingds
	 *
	 */
	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const
	{
		if (BatchElement.bUserDataIsColorVertexBuffer)
		{
			const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);
			FColorVertexBuffer* overrideColor = (FColorVertexBuffer*)(BatchElement.UserData);
			check(overrideColor);

			if (!LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
			{
				LocalVertexFactory->GetColorOverrideStream(overrideColor, VertexStreams);
			}
		}

		const FDeformMeshVertexFactory* deformMeshVertexFactory = (FDeformMeshVertexFactory*)(VertexFactory);

		//从deform mesh vertexFactory中读取需要的变量添加到shadering binding中
		const uint32 index = deformMeshVertexFactory->TransformIndex;
		ShaderBindings.Add(TransformIndex, index);
		FDeformMeshSceneProxy* deformProxy = deformMeshVertexFactory->SceneProxy;
		ShaderBindings.Add(TransformSRV, deformProxy->GetDeformTransformsSRV());
	}

private:
	/**
	 *
	 */
	LAYOUT_FIELD(FShaderParameter, TransformIndex);
	LAYOUT_FIELD(FShaderResourceParameter, TransformSRV);
};

IMPLEMENT_TYPE_LAYOUT(FDeformMeshVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FDeformMeshVertexFactory, SF_Vertex,
	FDeformMeshVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FDeformMeshVertexFactory,
	"/Plugin/CustomShaderModule/Private/LocalVertexFactory.ush", true, true, true, true, true);

static void InitOrUpdateResource(FRenderResource* Resource)
{
	if (!Resource->IsInitialized())
	{
		Resource->InitResource();
	}
	else
	{
		Resource->UpdateRHI();
	}
}

static void InitVertexFactoryData(FDeformMeshVertexFactory* VertexFactory,
	FStaticMeshVertexBuffers* VertexBuffer)
{
	ENQUEUE_RENDER_COMMAND(StaticMeshVertexBuffersLegacyInit)(
		[VertexFactory, VertexBuffer](FRHICommandListImmediate& RHICommandList)
		{
			InitOrUpdateResource(&VertexBuffer->PositionVertexBuffer);
			//InitOrUpdateResource(&RHICommandList)
			InitOrUpdateResource(&VertexBuffer->StaticMeshVertexBuffer);

			FLocalVertexFactory::FDataType Data;
			//VertexBuffer->PositionVertexBuffer
			VertexBuffer->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, Data);
			VertexBuffer->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, Data);
			VertexFactory->SetData(Data);

			InitOrUpdateResource(VertexFactory);
		}
		);
}

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

void UDeformMeshComponent::UpdateSectionTransform(int32 SectionIndex, const FTransform& DeformTransform)
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
					deformMeshSceneProxy->UpateDeformTransofm_RenderThread(SectionIndex, transformMatrix);
				}
			);
		}

		UpdateLocalBounds();
		MarkRenderTransformDirty();
	}
}

/// <summary>
/// 当我们完成更新section tranform的时候，调用这个以更新渲染线程相关uniform buffer
/// </summary>
void UDeformMeshComponent::FinishDeformUpdate()
{
	if (SceneProxy)
	{
		FDeformMeshSceneProxy* deformMeshSceneProxy = static_cast<FDeformMeshSceneProxy*>(SceneProxy);
		ENQUEUE_RENDER_COMMAND(FDeformMeshAllTransformSBUpdate)(
			[deformMeshSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				deformMeshSceneProxy->UpdateDeformTransformSB_RenderThread();
			});
	}
}

void UDeformMeshComponent::ClearSection(int32 SectionIndex)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		DeformMeshSections[SectionIndex].Reset();
		UpdateLocalBounds();
		MarkRenderTransformDirty();
	}
}

void UDeformMeshComponent::ClearAllMeshSections()
{
	DeformMeshSections.Empty();
	UpdateLocalBounds();
	MarkRenderTransformDirty();
}

int32 UDeformMeshComponent::GetNumMaterials() const
{
	return DeformMeshSections.Num();
}

FBoxSphereBounds UDeformMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds ret(LocalBounds.TransformBy(LocalToWorld));

	ret.BoxExtent *= BoundsScale;
	ret.SphereRadius *= BoundsScale;
	return ret;
}

FPrimitiveSceneProxy* UDeformMeshComponent::CreateSceneProxy()
{
	//Super::CreateSceneProxy();
	if (!SceneProxy)
	{
		return new FDeformMeshSceneProxy(this);
	}
	else
	{
		return SceneProxy;
	}
}

void UDeformMeshComponent::SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility)
{
	if (SectionIndex < DeformMeshSections.Num())
	{
		DeformMeshSections[SectionIndex].bSectionVisible = bNewVisibility;

		if (SceneProxy)
		{
			FDeformMeshSceneProxy* deformProxy = static_cast<FDeformMeshSceneProxy*>(SceneProxy);
			ENQUEUE_RENDER_COMMAND(FDeformMeshVisibilityUpdate)(
				[deformProxy, SectionIndex, bNewVisibility](FRHICommandListImmediate& RHICmdList)
				{
					deformProxy->SetSectionVisibility_RenderThread(SectionIndex, bNewVisibility);
				});
		}
	}
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

	// Update Global Bounds
	UpdateBounds();

	MarkRenderTransformDirty();
}
