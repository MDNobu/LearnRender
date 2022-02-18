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

void DrawTestShaderRenderTarget_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FTextureRenderTargetResource* OutputRenderTargetResource,
	ERHIFeatureLevel::Type FeatureLevel,
	FName TextureRenderTargetName,
	FLinearColor MyColor
)
{
	check(IsInRenderingThread());

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
	{
		

		FIntPoint DisplacementMapResolution(OutputRenderTargetResource->GetSizeX(), OutputRenderTargetResource->GetSizeY());

		// Update viewport.
		RHICmdList.SetViewport(
			0, 0, 0.f,
			DisplacementMapResolution.X, DisplacementMapResolution.Y, 1.f);

		// Get shaders.
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef< FMyTestVS > VertexShader(GlobalShaderMap);
		TShaderMapRef< FMyPS > PixelShader(GlobalShaderMap);

		//UE_LOG(LogTemp, Warning, TEXT("%s content"), *GetName());
		//GlobalShaderMap->
		
		// Set the graphic pipeline state.
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		//GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		PixelShader->SetColor(RHICmdList, MyColor);
		//SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), );
		//SetShaderValue(RHICmdList, )

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		// Update viewport.
		RHICmdList.SetViewport(
			0, 0, 0.f,
			OutputRenderTargetResource->GetSizeX(), OutputRenderTargetResource->GetSizeY(), 1.f);

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
		/*
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
		*/
		

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
		/* Shader Vertex Format(float3 pos, float2 uv) */
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

	UE_LOG(LogTemp, Warning, TEXT("DrawTestRenderTarget invoked"), *MyColor.ToString());

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
