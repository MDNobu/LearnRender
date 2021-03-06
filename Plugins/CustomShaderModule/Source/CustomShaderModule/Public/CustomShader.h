	// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "CustomShader.generated.h"

#pragma region DefineUniformBuffers



USTRUCT(BlueprintType)
struct FMyUniformStructData
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Test")
		FLinearColor ColorOne;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Test")
		FLinearColor ColorTwo;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Test")
		FLinearColor ColorThree;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Test")
		FLinearColor ColorFour;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Test")
	int32 ColorIndex;
};
#pragma endregion

class FMyTestVS : public FGlobalShader
{
	//DECLARE_EXPORTED_SHADER_TYPE(FMyTestVS, Global)
	DECLARE_SHADER_TYPE(FMyTestVS, Global)

public:
	FMyTestVS()
	{

	}

	FMyTestVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{

	}

	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MY_DEFINE"), 1);
	}
};


class FMyPS : public FGlobalShader
{
	//DECLARE_SHADER_TYPE(FMyPS, Global)

public:
	DECLARE_GLOBAL_SHADER(FMyPS);
	//SHADER_USE_PARAMETER_STRUCT(FMyPS, FGlobalShader);
	//// 和游戏线程的参数一一对应
	//BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	//	SHADER_PARAMETER(FVector4, InputColor)
	//END_SHADER_PARAMETER_STRUCT()

	FMyPS()
	{

	}

	FMyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		OutputColor.Bind(Initializer.ParameterMap, TEXT("MyColor"));
		//OutputColor.Bind(Initializer.ParameterMap, TEXT("MyColor"));
		TestTexture.Bind(Initializer.ParameterMap, TEXT("MyTexture"));
		TestTextureSampler.Bind(Initializer.ParameterMap, TEXT("MyTextureSampler"));
	}

	static bool ShouldCompilePermutation(const FShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		return true;
	}

	static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MY_DEFINE"), 1);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FLinearColor& InColor, 
		FTextureReferenceRHIRef InMyTexture, const FMyUniformStructData& InShaderStructData);

private:
	// 定义pixel shader输出的颜色
	LAYOUT_FIELD(FShaderParameter, OutputColor);

	LAYOUT_FIELD(FShaderResourceParameter, TestTexture);
	LAYOUT_FIELD(FShaderResourceParameter, TestTextureSampler);
};




UCLASS(MinimalAPI)
class UCustomShader_BPL : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable, Category = "CustomShader", meta = (WorldContext = "WorldContextObject"))
	static void DrawTestShaderRenderTarget(class UTextureRenderTarget2D* OutputRenderTarget,
		AActor* Ac, FLinearColor Color, UTexture2D* MyTexture);
};
//void RenderMyTest2(FRHICommandListImmediate& RHICmdList, FTextureRenderTargetResource* OutputRenderTargetResource);