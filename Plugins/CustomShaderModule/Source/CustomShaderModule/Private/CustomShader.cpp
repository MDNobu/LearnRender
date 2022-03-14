// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomShader.h"
#include "GlobalShader.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Runtime/RenderCore/Public/PixelShaderUtils.h"



IMPLEMENT_SHADER_TYPE(, FMyTestVS, TEXT("/Plugin/CustomShaderModule/Private/MyTest.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FMyPS, TEXT("/Plugin/CustomShaderModule/Private/MyTest.usf"), TEXT("MainPS"), SF_Pixel);


//顶点shader输入
struct FColorVertex {
	FVector2D Position;
	FVector4 Color;
};
//顶点缓冲区
class FSimpleVertexBuffer : public FVertexBuffer
{
public:
	void InitRHI() override
	{
		TResourceArray<FColorVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(3);

		Vertices[0].Position = FVector2D(0, 0.75);
		Vertices[0].Color = FVector4(1, 0, 0, 1);

		Vertices[1].Position = FVector2D(0.75, -0.75);
		Vertices[1].Color = FVector4(0, 1, 0, 1);

		Vertices[2].Position = FVector2D(-0.75, -0.75);
		Vertices[2].Color = FVector4(0, 0, 1, 1);

		FRHIResourceCreateInfo CreateInfo(&Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}


};

TGlobalResource<FSimpleVertexBuffer> GSimpleVertexBuffer;
//顶点缓冲区属性描述
class FColorVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FColorVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FColorVertex, Position), VET_Float2, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FColorVertex, Color), VET_Float4, 1, Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FColorVertexDeclaration> GSimpleVertexDeclaration;
//索引缓冲区
class FSimpleIndexBuffer : public FIndexBuffer
{
public:
	void InitRHI() override
	{
		const uint16 Indices[] = { 0, 1, 2 };

		TResourceArray <uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		uint32 NumIndices = ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

		FRHIResourceCreateInfo CreateInfo(&IndexBuffer);
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};

TGlobalResource<FSimpleIndexBuffer> GSimpleIndexBuffer;

 
struct FTextureVertex
{
public:
	FVector4 Position;
	FVector2D UV;
};


/************************************************************************/
/* Simple static vertex buffer.                                         */
/************************************************************************/
class FSimpleScreenVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI()
	{
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(6);

		Vertices[0].Position = FVector4(-1, 1, 0, 1);
		Vertices[0].UV = FVector2D(0, 0);

		Vertices[1].Position = FVector4(1, 1, 0, 1);
		Vertices[1].UV = FVector2D(1, 0);

		Vertices[2].Position = FVector4(-1, -1, 0, 1);
		Vertices[2].UV = FVector2D(0, 1);

		Vertices[3].Position = FVector4(1, -1, 0, 1);
		Vertices[3].UV = FVector2D(1, 1);

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};
TGlobalResource<FSimpleScreenVertexBuffer> GSimpleScreenVertexBuffer;


void RenderMyTest(FRHICommandListImmediate& InRHICmdList,
	FTextureRenderTargetResource* OuttRenderTargetResource, FLinearColor InMyColor)
{
	FIntPoint DisplacementMapResolution(OuttRenderTargetResource->GetSizeX(), OuttRenderTargetResource->GetSizeY());

	// Update viewport.
	InRHICmdList.SetViewport(
		0, 0, 0.f,
		DisplacementMapResolution.X, DisplacementMapResolution.Y, 1.f);

	// Get shaders.
	//FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);  
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef< FMyTestVS > VertexShader(GlobalShaderMap);
	TShaderMapRef< FMyPS > PixelShader(GlobalShaderMap);

	//UE_LOG(LogTemp, Warning, TEXT("%s content"), *GetName());
	//GlobalShaderMap->

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	InRHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	//GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
	SetGraphicsPipelineState(InRHICmdList, GraphicsPSOInit);

	PixelShader->SetColor(InRHICmdList, InMyColor);
	//SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), );
	//SetShaderValue(RHICmdList, )

	

	// Update viewport.
	/*InRHICmdList.SetViewport(
		0, 0, 0.f,
		OuttRenderTargetResource->GetSizeX(), OuttRenderTargetResource->GetSizeY(), 1.f);*/
	FRHIResourceCreateInfo resouceCreateInfo;
	static const uint32 VertexSize = sizeof(FVector4) * 4;
	FVertexBufferRHIRef vertexBufferRHI = RHICreateVertexBuffer(VertexSize, BUF_Static, resouceCreateInfo);
	void* voidPtr = RHILockVertexBuffer(vertexBufferRHI, 0, VertexSize, RLM_WriteOnly);
	FVector4* vertices  =reinterpret_cast<FVector4*>(voidPtr);
	vertices[0] = FVector4(-1.f, 1.f, 0.f, 1.f);
	vertices[1] = FVector4(1.f, 1.f, 0.f, 1.f);
	vertices[2] = FVector4(-1.f, -1.f, 0.f, 1.f);
	vertices[3] = FVector4(1.f, -1.f, 0.f, 1.f);
	//FMemory::Memcpy(voidPtr, (void*)vertices, VertexSize);

	RHIUnlockVertexBuffer(vertexBufferRHI);

#pragma region VertexBufferWay2

#pragma endregion


	InRHICmdList.SetStreamSource(0, vertexBufferRHI, 0);
	//InRHICmdList.SetStreamSource(0, GSimpleScreenVertexBuffer.VertexBufferRHI, 0);
	InRHICmdList.DrawPrimitive(0, 2, 1);

	// Resolve render target
	InRHICmdList.CopyToResolveTarget(OuttRenderTargetResource->GetRenderTargetTexture(), 
		OuttRenderTargetResource->TextureRHI, FResolveParams());

}


void RenderMyTest2(FRHICommandListImmediate& RHICmdList, 
	FTextureRenderTargetResource* OutputRenderTargetResource, FLinearColor InColor)
{
	// Get shaders.
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FMyTestVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FMyPS> PixelShader(GlobalShaderMap);

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSimpleVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	PixelShader->SetColor(RHICmdList, InColor);

	// Update viewport.
	RHICmdList.SetViewport(
		0, 0, 0.f,
		OutputRenderTargetResource->GetSizeX(), OutputRenderTargetResource->GetSizeY(), 1.f);

	// Set the vertextBuffer.
	RHICmdList.SetStreamSource(0, GSimpleVertexBuffer.VertexBufferRHI, 0);

	RHICmdList.DrawIndexedPrimitive(
		GSimpleIndexBuffer.IndexBufferRHI,
		/*BaseVertexIndex=*/ 0,
		/* MinIndex = */  0,
		/* NumVertices = */3,
		/*StartIndex=*/0,
		/* NumPrimitives = */1,
		/*NumInstances=*/1);
}

void DrawTestShaderRenderTarget_RenderThread2(
	FRHICommandListImmediate& RHICmdList,
	FTextureRenderTargetResource* OutTextureRenderTargetResource,
	ERHIFeatureLevel::Type FeatureLevel,
	FName TextureRenderTargetName,
	FLinearColor MyColor
)
{
	check(IsInRenderingThread());

#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	TextureRenderTargetName.ToString(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("LensDistortionDisplacementGeneration %s"), *EventName);
#else
	SCOPED_DRAW_EVENT(RHICmdList, DrawUVDisplacementToRenderTarget_RenderThread);
#endif

	FRHITexture2D* RenderTargetTexture = OutTextureRenderTargetResource->GetRenderTargetTexture();

	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTexture, ERHIAccess::SRVMask, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(RenderTargetTexture, ERenderTargetActions::DontLoad_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("DrawCustomShader"));
	{
		FIntPoint DisplacementMapResolution(OutTextureRenderTargetResource->GetSizeX(),
			OutTextureRenderTargetResource->GetSizeY());

		// Update viewport.
		RHICmdList.SetViewport(
			0, 0, 0.f,
			DisplacementMapResolution.X, DisplacementMapResolution.Y, 1.f);

		// Get shaders.
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef< FMyTestVS > VertexShader(GlobalShaderMap);
		TShaderMapRef< FMyPS > PixelShader(GlobalShaderMap);

		// Set the graphic pipeline state.
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		// Update viewport.
		RHICmdList.SetViewport(
			0, 0, 0.f,
			OutTextureRenderTargetResource->GetSizeX(), OutTextureRenderTargetResource->GetSizeY(), 1.f);

		// Update shader uniform parameters.
		//VertexShader->SetParameters(RHICmdList, VertexShader.GetVertexShader(), CompiledCameraModel, DisplacementMapResolution);
		//PixelShader->SetParameters(RHICmdList, PixelShader.GetPixelShader(), CompiledCameraModel, DisplacementMapResolution);
		PixelShader->SetColor(RHICmdList, MyColor);


		// Draw
		RHICmdList.SetStreamSource(0, GSimpleScreenVertexBuffer.VertexBufferRHI, 0);
		// Draw grid.
		//uint32 PrimitiveCount = kGridSubdivisionX * kGridSubdivisionY * 2;
		RHICmdList.DrawPrimitive(0, 2, 1);
	}
	RHICmdList.EndRenderPass();
	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTexture, ERHIAccess::RTV, ERHIAccess::SRVMask));
}

void DrawTestShaderRenderTarget_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FTextureRenderTargetResource* OutputRenderTargetResource,
	ERHIFeatureLevel::Type FeatureLevel,
	FName TextureRenderTargetName,
	FLinearColor MyColor
)
{
	check(IsInRenderingThread());

	if (false)
	{
		DrawTestShaderRenderTarget_RenderThread2(RHICmdList, OutputRenderTargetResource, 
			FeatureLevel, TextureRenderTargetName, MyColor);
		return;
	}

#if WANTS_DRAW_MESH_EVENTS  
	FString EventName;
	TextureRenderTargetName.ToString(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("ShaderTest %s"), *EventName);
#else  
	SCOPED_DRAW_EVENT(RHICmdList, DrawTestShaderRenderTarget_RenderThread);
#endif  


	FRHITexture2D* RenderTargetTexture = OutputRenderTargetResource->GetRenderTargetTexture();

	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTexture, ERHIAccess::SRVMask, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(RenderTargetTexture, ERenderTargetActions::DontLoad_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("DrawCustomShader"));

	// 第一种尝试实现
	/*
	{
		



		// Create VertexBuffer and setup
		//static const uint32 VERTEX_SIZE = sizeof(FVector4) * 4;
		//FRHIResourceCreateInfo CreateInfo;
		//FVertexBufferRHIRef VertexBufferRHI = RHICreateVertexBuffer(VERTEX_SIZE, BUF_Static, CreateInfo);
		//void* VoidPtr = RHILockVertexBuffer(VertexBufferRHI, 0, VERTEX_SIZE, RLM_WriteOnly);

		//FVector4 Vertices[4];
		//Vertices[0].Set(-1.0f, 1.0f, 0, 1.0f);
		//Vertices[1].Set(1.0f, 1.0f, 0, 1.0f);
		//Vertices[2].Set(-1.0f, -1.0f, 0, 1.0f);
		//Vertices[3].Set(1.0f, -1.0f, 0, 1.0f);
		//FMemory::Memcpy(VoidPtr, (void*)Vertices, VERTEX_SIZE);
		//RHIUnlockVertexBuffer(VertexBufferRHI);

		// Update shader uniform parameters.
		//PixelShader->SetParameters(RHICmdList, MyColor);


		// Draw grid.
		//RHICmdList.DrawPrimitive(0, PrimitiveCount, 1);

#pragma region DrawQuad
		FVertexDeclarationElementList Elements;
		const auto Stride = sizeof(FTextureVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, UV), VET_Float2, 1, Stride));
		FVertexDeclarationRHIRef VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDeclarationRHI;

		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);


		FRHIResourceCreateInfo CreateInfo;
		FVertexBufferRHIRef VertexBufferRHI = RHICreateVertexBuffer(sizeof(FTextureVertex) * 4, BUF_Volatile, CreateInfo);
		void* VoidPtr = RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FTextureVertex) * 4, RLM_WriteOnly);

		FTextureVertex* Vertices = (FTextureVertex*)VoidPtr;
		Vertices[0].Position = FVector4(-1.0f, 1.0f, 0, 1.0f);
		Vertices[1].Position = FVector4(1.0f, 1.0f, 0, 1.0f);
		Vertices[2].Position = FVector4(-1.0f, -1.0f, 0, 1.0f);
		Vertices[3].Position = FVector4(1.0f, -1.0f, 0, 1.0f);
		Vertices[0].UV = FVector2D(0, 0);
		Vertices[1].UV = FVector2D(1, 0);
		Vertices[2].UV = FVector2D(0, 1);
		Vertices[3].UV = FVector2D(1, 1);
		RHIUnlockVertexBuffer(VertexBufferRHI);

		//SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
		RHICmdList.DrawPrimitive(0, 2, 1);
#pragma endregion



		// Get the collection of Global Shaders
		//auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		//// Get the actual shader instances off the ShaderMap
		//TShaderMapRef<FMyTestVS> MyVertexShader(ShaderMap);
		//TShaderMapRef<FMyPS> MyPixelShader(ShaderMap);

		// Set Shader Param
		//PixelShader->SetColor(RHICmdList, MyColor);

		// Steup PSO(include shader state, raster state, blend state, depth stencil state etc.)
		//FGraphicsPipelineStateInitializer GraphicsPSOInit;
		//RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		// Set Shader State
		Shader Vertex Format(float3 pos, float2 uv)
		//GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();

		//GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		//GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		// Set RasterState
		//GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();

		// Set BlendState
		//GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		// Set DepthStencilState
		//GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<>::GetRHI();

		// Set Primitive Type
		//GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

		//SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);



		//RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
		RHICmdList.SetStreamSource(0, GSimpleScreenVertexBuffer.VertexBufferRHI, 0);
		RHICmdList.DrawPrimitive(0, 2, 1);
	}*/

	{
		//RenderMyTest(RHICmdList, OutputRenderTargetResource, MyColor);
	}

	//第二种尝试实现
	{
		RenderMyTest2(RHICmdList, OutputRenderTargetResource, MyColor);
	}
	RHICmdList.EndRenderPass();

	RHICmdList.Transition(FRHITransitionInfo(RenderTargetTexture, ERHIAccess::RTV, ERHIAccess::SRVMask));
}


void UCustomShader_BPL::DrawTestShaderRenderTarget(
	UTextureRenderTarget2D* OutputRenderTarget, AActor* Ac, FLinearColor MyColor)
{

	check(IsInGameThread());

	if (!OutputRenderTarget)
	{
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("DrawTestRenderTarget invoked"), *MyColor.ToString());

	FTextureRenderTargetResource* TextureRenderTargetResource =
		OutputRenderTarget->GameThread_GetRenderTargetResource();
	UWorld* World = Ac->GetWorld();
	ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
	FName TextureRenderTargetName = OutputRenderTarget->GetFName();
	ENQUEUE_RENDER_COMMAND(CaptureCommand)(
		[TextureRenderTargetResource, FeatureLevel, MyColor, TextureRenderTargetName](FRHICommandListImmediate& RHICmdList)
		{
			DrawTestShaderRenderTarget_RenderThread(RHICmdList, TextureRenderTargetResource, FeatureLevel, TextureRenderTargetName, MyColor);
		}
	);


}
