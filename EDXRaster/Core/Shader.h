#pragma once

#include "RenderState.h"
#include "Math/Vector.h"
#include "Graphics/Texture.h"
#include "Graphics/Color.h"
#include "SIMD/SSE.h"

namespace EDX
{
	namespace RasterRenderer
	{
		struct ProjectedVertex
		{
			Vector4 projectedPos;
			float	invW;
			Vector3 position;
			Vector3 normal;
			Vector2 texCoord;

			ProjectedVertex()
				: invW(0.0f)
			{
			}
		};

		class VertexShader
		{
		public:
			virtual ~VertexShader() {}
			virtual void Execute(const RenderState& renderState,
				const Vector3& vPosIn,
				const Vector3& vNormalIn,
				const Vector2& vTexIn,
				ProjectedVertex* pOut) = 0;
		};

		class DefaultVertexShader : public VertexShader
		{
		public:
			virtual void Execute(const RenderState& renderState,
				const Vector3& vPosIn,
				const Vector3& vNormalIn,
				const Vector2& vTexIn,
				ProjectedVertex* pOut)
			{
				pOut->projectedPos = Matrix::TransformPoint(Vector4(vPosIn.x, vPosIn.y, vPosIn.z, 1.0f), renderState.GetModelViewProjMatrix());
				pOut->position = vPosIn;
				pOut->normal = vNormalIn;
				pOut->texCoord = vTexIn;
			}
		};

		struct Fragment
		{
			Vector3 position;
			Vector3 normal;
			Vector2 texCoord;
			float depth;

			void Interpolate(const ProjectedVertex& v0,
				const ProjectedVertex& v1,
				const ProjectedVertex& v2,
				float& b0,
				float& b1)
			{
				float b2 = 1.0f - b0 - b1;
				b0 *= v0.invW;
				b1 *= v1.invW;
				b2 *= v2.invW;
				float invB = 1.0f / (b0 + b1 + b2);
				b0 *= invB;
				b1 *= invB;
				b2 = 1.0f - b0 - b1;

				position = b0 * v0.position + b1 * v1.position + b2 * v2.position;
				normal = b0 * v0.normal + b1 * v1.normal + b2 * v2.normal;
				texCoord = b0 * v0.texCoord + b1 * v1.texCoord + b2 * v2.texCoord;
			}

			float GetDepth(const ProjectedVertex& v0,
				const ProjectedVertex& v1,
				const ProjectedVertex& v2,
				float b0,
				float b1)
			{
				float b2 = 1.0f - b0 - b1;
				depth = b0 * v0.projectedPos.z + b1 * v1.projectedPos.z + b2 * v2.projectedPos.z;

				return depth;
			}
		};

		struct QuadFragment
		{
			Vec3f_SSE position;
			Vec3f_SSE normal;
			Vec2f_SSE texCoord;
			FloatSSE depth[1];

			int vId0 : 24, vId1 : 24, vId2 : 24;
			int textureId : 24;
			FloatSSE lambda0, lambda1;
			Vector2i pixelCoord;
			BoolSSE coverageMask[1];

			void Interpolate(const ProjectedVertex& v0,
				const ProjectedVertex& v1,
				const ProjectedVertex& v2,
				FloatSSE& b0,
				FloatSSE& b1)
			{
				const auto One = FloatSSE(Math::EDX_ONE);
				FloatSSE b2 = One - b0 - b1;
				b0 *= v0.invW;
				b1 *= v1.invW;
				b2 *= v2.invW;
				FloatSSE invB = One / (b0 + b1 + b2);
				b0 *= invB;
				b1 *= invB;
				b2 = One - b0 - b1;

				position = b0 * Vec3f_SSE(v0.position.x, v0.position.y, v0.position.z) + b1 * Vec3f_SSE(v1.position.x, v1.position.y, v1.position.z) + b2 * Vec3f_SSE(v2.position.x, v2.position.y, v2.position.z);
				normal = b0 * Vec3f_SSE(v0.normal.x, v0.normal.y, v0.normal.z) + b1 * Vec3f_SSE(v1.normal.x, v1.normal.y, v1.normal.z) + b2 * Vec3f_SSE(v2.normal.x, v2.normal.y, v2.normal.z);
				texCoord = b0 * Vec2f_SSE(v0.texCoord.x, v0.texCoord.y) + b1 * Vec2f_SSE(v1.texCoord.x, v1.texCoord.y) + b2 * Vec2f_SSE(v2.texCoord.x, v2.texCoord.y);
			}

			FloatSSE GetDepth(const ProjectedVertex& v0,
				const ProjectedVertex& v1,
				const ProjectedVertex& v2,
				const uint sId,
				const FloatSSE& b0,
				const FloatSSE& b1)
			{
				const auto One = FloatSSE(Math::EDX_ONE);
				FloatSSE b2 = One - b0 - b1;
				depth[sId] = b0 * v0.projectedPos.z + b1 * v1.projectedPos.z + b2 * v2.projectedPos.z;

				return depth[sId];
			}
		};

		class PixelShader
		{
		public:
			virtual ~PixelShader() {}
			virtual Color Shade(const Fragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir) const = 0;
		};

		class BlinnPhongPixelShader : public PixelShader
		{
		public:
			Color Shade(const Fragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir) const
			{
				Vector3 normal = Math::Normalize(fragIn.normal);
				float diffuseAmount = Math::Saturate(Math::Dot(Math::Normalize(lightDir), normal));
				Color diffuse = (diffuseAmount + 0.1f) * 2 * Color::WHITE * Math::EDX_INV_PI;

				Vector3 eyeDir = Math::Normalize(eyePos - fragIn.position);
				Vector3 halfVec = Math::Normalize(lightDir + eyeDir);
				float specularAmount = Math::Saturate(Math::Dot(normal, halfVec));
				specularAmount = Math::Pow(specularAmount, max(200.0f, 0.0001f)) * 2;
				Color specular = Color::WHITE * specularAmount;

				return diffuse + specular;
			}
		};

		class QuadPixelShader
		{
		public:
			virtual ~QuadPixelShader() {}
			virtual Vec3f_SSE Shade(const QuadFragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				RenderState& state) const = 0;
		};

		class QuadLambertianPixelShader : public QuadPixelShader
		{
		public:
			Vec3f_SSE Shade(const QuadFragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				RenderState& state) const
			{
				FloatSSE w = SSE::Rsqrt(Math::Dot(fragIn.normal, fragIn.normal));
				Vec3f_SSE normal = fragIn.normal * w;
				Vec3f_SSE vecLightDir = Vec3f_SSE(Math::Normalize(lightDir));

				FloatSSE diffuseAmount = Math::Dot(vecLightDir, normal);
				BoolSSE mask = diffuseAmount < FloatSSE(Math::EDX_ZERO);
				diffuseAmount = SSE::Select(mask, FloatSSE(Math::EDX_ZERO), diffuseAmount);
				FloatSSE diffuse = (diffuseAmount + 0.2f) * 2 * Math::EDX_INV_PI;

				return diffuse;
			}
		};

		class QuadLambertianAlbedoPixelShader : public QuadPixelShader
		{
		public:
			Vec3f_SSE Shade(const QuadFragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				RenderState& state) const
			{
				FloatSSE w = SSE::Rsqrt(Math::Dot(fragIn.normal, fragIn.normal));
				Vec3f_SSE normal = fragIn.normal * w;
				Vec3f_SSE vecLightDir = Vec3f_SSE(Math::Normalize(lightDir));

				FloatSSE diffuseAmount = Math::Dot(vecLightDir, normal);
				BoolSSE mask = diffuseAmount < FloatSSE(Math::EDX_ZERO);
				diffuseAmount = SSE::Select(mask, FloatSSE(Math::EDX_ZERO), diffuseAmount);
				Vec3f_SSE quadAlbedo;
				for (auto i = 0; i < 4; i++)
				{
					Color color = state.mTextures[fragIn.textureId]->Sample(Vector2(fragIn.texCoord.u[i], fragIn.texCoord.v[i]));
					quadAlbedo.x[i] = color.r;
					quadAlbedo.y[i] = color.g;
					quadAlbedo.z[i] = color.b;
				}
				FloatSSE diffuse = (diffuseAmount + 0.2f) * 2 * Math::EDX_INV_PI;

				return diffuse * quadAlbedo;
			}
		};

		class QuadBlinnPhongPixelShader : public QuadPixelShader
		{
		public:
			Vec3f_SSE Shade(const QuadFragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				RenderState& state) const
			{
				FloatSSE w = SSE::Rsqrt(Math::Dot(fragIn.normal, fragIn.normal));
				Vec3f_SSE normal = fragIn.normal * w;
				Vec3f_SSE vecLightDir = Vec3f_SSE(Math::Normalize(lightDir));

				FloatSSE diffuseAmount = Math::Dot(vecLightDir, normal);
				BoolSSE mask = diffuseAmount < FloatSSE(Math::EDX_ZERO);
				diffuseAmount = SSE::Select(mask, FloatSSE(Math::EDX_ZERO), diffuseAmount);

				FloatSSE diffuse = (diffuseAmount + 0.2f) * 2 * Math::EDX_INV_PI;

				Vec3f_SSE eyeDir = Vec3f_SSE(eyePos) - fragIn.position;
				w = SSE::Rsqrt(Math::Dot(eyeDir, eyeDir));
				eyeDir *= w;

				Vec3f_SSE halfVec = vecLightDir + eyeDir;
				w = SSE::Rsqrt(Math::Dot(halfVec, halfVec));
				halfVec *= w;

				FloatSSE specularAmount = Math::Dot(normal, halfVec);
				specularAmount = FloatSSE(Math::Pow(specularAmount[0], max(200.0f, 0.0001f)) * 2,
					Math::Pow(specularAmount[1], max(200.0f, 0.0001f)) * 2,
					Math::Pow(specularAmount[2], max(200.0f, 0.0001f)) * 2,
					Math::Pow(specularAmount[3], max(200.0f, 0.0001f)) * 2);

				return diffuse + specularAmount;
			}
		};
	}
}