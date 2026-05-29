#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"

// モデル描画用テクスチャ
Texture2D g_baseTex       : register(t0);   // ベースカラーテクスチャ
Texture2D g_metalRoughTex : register(t1);   // メタリック(b)/ラフネス(g)テクスチャ
Texture2D g_emissiveTex   : register(t2);   // 発光テクスチャ
Texture2D g_normalTex     : register(t3);   // 法線マップ

// 特殊処理用テクスチャ
Texture2D g_dirShadowMap  : register(t10);  // 平行光シャドウマップ
Texture2D g_dissolveTex   : register(t11);  // ディゾルブマップ
Texture2D g_environmentTex: register(t12);  // 反射景マップ

// サンプラ
SamplerState           g_ss    : register(s0); // 通常テクスチャ用
SamplerComparisonState g_ssCmp : register(s1); // 比較サンプラ（シャドウ用）

//=============================================================
// 定数
//=============================================================
static const float PI = 3.14159265358979f;

//=============================================================
// Poisson Disk サンプル点（16点）
//=============================================================
static const float2 g_PoissonDisk[16] =
{
	float2(-0.94201624f, -0.39906216f),
	float2( 0.94558609f, -0.76890725f),
	float2(-0.09418410f, -0.92938870f),
	float2( 0.34495938f,  0.29387760f),
	float2(-0.91588581f,  0.45771432f),
	float2(-0.81544232f, -0.87912464f),
	float2(-0.38277543f,  0.27676845f),
	float2( 0.97484398f,  0.75648379f),
	float2( 0.44323325f, -0.97511554f),
	float2( 0.53742981f, -0.47373420f),
	float2(-0.26496911f, -0.41893023f),
	float2( 0.79197514f,  0.19090188f),
	float2(-0.24188840f,  0.99706507f),
	float2(-0.81409955f,  0.91437590f),
	float2( 0.19984126f,  0.78641367f),
	float2( 0.14383161f, -0.14100790f),
};

//=============================================================
// PBR ヘルパー関数 (Cook-Torrance GGX)
//=============================================================

// GGX 法線分布関数 (NDF)
float D_GGX(float NdotH, float roughness)
{
	float a  = roughness * roughness;
	float a2 = a * a;
	float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
	return a2 / (PI * d * d + 1e-7f);
}

// Smith GGX 幾何減衰 (Schlick 近似)
float G_SmithGGX(float NdotV, float NdotL, float roughness)
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f;
	float gv = NdotV / (NdotV * (1.0f - k) + k + 1e-7f);
	float gl = NdotL / (NdotL * (1.0f - k) + k + 1e-7f);
	return gv * gl;
}

// Schlick フレネル近似
float3 F_Schlick(float VdotH, float3 F0)
{
	return F0 + (1.0f - F0) * pow(saturate(1.0f - VdotH), 5.0f);
}

// Cook-Torrance BRDF で1方向ライトの輝度を返す
// lightDir : 光源への方向（正規化済み）
// viewDir  : カメラへの方向（正規化済み）
// N        : ワールド法線（正規化済み）
// albedo   : 拡散ベースカラー
// F0       : フレネル反射率（金属なら baseColor、非金属なら 0.04）
// roughness, metallic
float3 CookTorrance(float3 lightDir, float3 viewDir, float3 N,
					float3 albedo, float3 F0, float roughness, float metallic,
					float3 lightColor, float attenuation)
{
	float3 H     = normalize(viewDir + lightDir);
	float NdotL  = saturate(dot(N, lightDir));
	float NdotV  = saturate(dot(N, viewDir));
	float NdotH  = saturate(dot(N, H));
	float VdotH  = saturate(dot(viewDir, H));

	// Specular BRDF
	float  D  = D_GGX(NdotH, roughness);
	float  G  = G_SmithGGX(NdotV, NdotL, roughness);
	float3 F  = F_Schlick(VdotH, F0);
	float3 specular = (D * G * F) / (4.0f * NdotV * NdotL + 1e-7f);

	// Diffuse: 金属は拡散なし
	float3 kD = (1.0f - F) * (1.0f - metallic);
	float3 diffuse = kD * albedo / PI;

	return (diffuse + specular) * lightColor * NdotL * attenuation;
}

//=============================================================
// PCSS ソフトシャドウ（Poisson 16点）
//=============================================================
float CalcShadow(float3 wPos, float3 wN)
{
	// 法線オフセットバイアス（シャドウアクネ対策）
	float3 offsetPos = wPos + wN * 0.05f;
	float4 liPos = mul(float4(offsetPos, 1.0f), g_DL_mLightVP);
	liPos.xyz /= liPos.w;

	// シャドウマップ範囲外は影なし
	if (abs(liPos.x) > 1.0f || abs(liPos.y) > 1.0f || liPos.z > 1.0f)
		return 1.0f;

	float2 uv = liPos.xy * float2(1.0f, -1.0f) * 0.5f + 0.5f;
	float  z  = liPos.z;

	float w, h;
	g_dirShadowMap.GetDimensions(w, h);
	float2 texelSize = float2(1.0f / w, 1.0f / h);

	// ソフトネス半径（テクセル単位）
	static const float k_SoftRadius = 2.5f;

	float shadow = 0.0f;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 offset = g_PoissonDisk[i] * texelSize * k_SoftRadius;
		shadow += g_dirShadowMap.SampleCmpLevelZero(g_ssCmp, uv + offset, z);
	}
	return shadow / 16.0f;
}

//=============================================================
// ピクセルシェーダ
//=============================================================
float4 main(VSOutput In) : SV_Target0
{
	//------------------------------------------
	// ディゾルブ
	//------------------------------------------
	float discardValue = g_dissolveTex.Sample(g_ss, In.UV).r;
	if (discardValue < g_dissolveValue)
		discard;

	//------------------------------------------
	// ベースカラー
	//------------------------------------------
	float4 baseColor;
	if (g_UseTriplanar)
	{
		float3 blend = pow(abs(normalize(In.wN)), 4.0f);
		blend /= (blend.x + blend.y + blend.z + 1e-5f);
		float3 scaledPos = In.wPos * g_TriplanarScale;
		float4 cx = g_baseTex.Sample(g_ss, scaledPos.yz);
		float4 cy = g_baseTex.Sample(g_ss, scaledPos.xz);
		float4 cz = g_baseTex.Sample(g_ss, scaledPos.xy);
		baseColor = (cx * blend.x + cy * blend.y + cz * blend.z) * g_BaseColor * In.Color;
	}
	else
	{
		baseColor = g_baseTex.Sample(g_ss, In.UV) * g_BaseColor * In.Color;
	}
	if (baseColor.a < 0.05f)
		discard;

	//------------------------------------------
	// カメラ方向
	//------------------------------------------
	float3 vCam    = g_CamPos - In.wPos;
	float  camDist = length(vCam);
	vCam = normalize(vCam);

	//------------------------------------------
	// ワールド法線（法線マップ適用）
	//------------------------------------------
	float3 wN = g_normalTex.Sample(g_ss, In.UV).rgb * 2.0f - 1.0f;
	{
		row_major float3x3 mTBN =
		{
			normalize(In.wT),
			normalize(In.wB),
			normalize(In.wN),
		};
		wN = mul(wN, mTBN);
	}
	wN = normalize(wN);

	//------------------------------------------
	// PBR マテリアルパラメータ
	//------------------------------------------
	float4 mr       = g_metalRoughTex.Sample(g_ss, In.UV);
	float  metallic  = saturate(mr.b * g_Metallic);
	float  roughness = saturate(mr.g * g_Roughness);
	roughness = max(roughness, 0.04f); // 完全鏡面防止

	// F0: 非金属=0.04、金属=ベースカラー
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor.rgb, metallic);
	// 拡散アルベド: 金属は0
	float3 albedo = baseColor.rgb * (1.0f - metallic);

	//------------------------------------------
	// シャドウ（法線オフセット PCSS）
	//------------------------------------------
	float shadow = CalcShadow(In.wPos, wN);

	//------------------------------------------
	// ライティング計算
	//------------------------------------------
	float3 outColor = 0.0f;

	// ---- 平行光 (PBR Cook-Torrance) ----
	{
		float3 lightDir = normalize(-g_DL_Dir);
		outColor += CookTorrance(lightDir, vCam, wN,
								 albedo, F0, roughness, metallic,
								 g_DL_Color, 1.0f) * shadow;
	}

	// ---- 点光 (PBR Cook-Torrance) ----
	float totalBrightness = g_AmbientLight.a;
	for (int i = 0; i < g_PointLightNum.x; ++i)
	{
		float3 toLight = g_PointLights[i].Pos - In.wPos;
		float  dist    = length(toLight);
		if (dist >= g_PointLights[i].Radius)
			continue;

		float3 lightDir = toLight / dist;

		// 距離減衰（逆二乗）
		float  atte = 1.0f - saturate(dist / g_PointLights[i].Radius);
		atte *= atte;

		// 明度ライト寄与
		float atteFull = 1.0f - saturate(dist / g_PointLights[i].Radius);
		totalBrightness += (1.0f - pow(1.0f - atteFull, 2.0f)) * g_PointLights[i].IsBright;

		outColor += CookTorrance(lightDir, vCam, wN,
								 albedo, F0, roughness, metallic,
								 g_PointLights[i].Color, atte);
	}

	// ---- 半球環境光（空色 / 地面色で影が黒くなるのを防ぐ）----
	{
		float hemi    = dot(wN, float3(0.0f, 1.0f, 0.0f)) * 0.5f + 0.5f;
		float3 ambient = lerp(g_AmbientLight.rgb * 0.4f,
							  g_AmbientLight.rgb,
							  hemi);
		outColor += ambient * baseColor.rgb * baseColor.a;
	}

	// ---- エミッシブ ----
	float3 emissive = g_emissiveTex.Sample(g_ss, In.UV).rgb * g_Emissive * In.Color.rgb;
	if (g_OnlyEmissie)
		outColor = emissive;
	else
		outColor += emissive;

	//------------------------------------------
	// フォグ
	//------------------------------------------
	if (g_HeightFogEnable && g_FogEnable)
	{
		if (In.wPos.y < g_HeightFogTopValue)
		{
			float distRate   = saturate(length(In.wPos - g_CamPos) / g_HeightFogDistance);
			distRate = pow(distRate, 2.0f);
			float heightRange = g_HeightFogTopValue - g_HeightFogBottomValue;
			float heightRate  = 1.0f - saturate((In.wPos.y - g_HeightFogBottomValue) / heightRange);
			outColor = lerp(outColor, g_HeightFogColor, heightRate * distRate);
		}
	}
	if (g_DistanceFogEnable && g_FogEnable)
	{
		float f = saturate(1.0f / exp(camDist * g_DistanceFogDensity));
		outColor = lerp(g_DistanceFogColor, outColor, f);
	}

	//------------------------------------------
	// ディゾルブ輪郭発光
	//------------------------------------------
	if (g_dissolveValue > 0.0f)
	{
		if (abs(discardValue - g_dissolveValue) < g_dissolveEdgeRange)
			outColor += g_dissolveEmissive;
	}

	//------------------------------------------
	// 明度クランプ
	//------------------------------------------
	totalBrightness = saturate(totalBrightness);
	outColor *= totalBrightness;

	return float4(outColor, baseColor.a);
}
