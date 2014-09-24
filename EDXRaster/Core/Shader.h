#pragma once

#include "RenderStates.h"
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
			virtual void Execute(const Vector3& vPosIn,
				const Vector3& vNormalIn,
				const Vector2& vTexIn,
				ProjectedVertex* pOut) = 0;
		};

		class DefaultVertexShader : public VertexShader
		{
		public:
			virtual void Execute(const Vector3& vPosIn,
				const Vector3& vNormalIn,
				const Vector2& vTexIn,
				ProjectedVertex* pOut)
			{
				pOut->projectedPos = Matrix::TransformPoint(Vector4(vPosIn.x, vPosIn.y, vPosIn.z, 1.0f), RenderStates::Instance()->GetModelViewProjMatrix());
				pOut->position = vPosIn;
				pOut->normal = vNormalIn;
				pOut->texCoord = vTexIn;
			}
		};

		struct CoverageMask
		{
			int bits[4]; // For up to 32x MSAA coverage mask
			inline CoverageMask()
			{
				bits[0] = 0;
				bits[1] = 0;
				bits[2] = 0;
				bits[3] = 0;
			}
			inline CoverageMask(const BoolSSE& mask, uint sampleId)
				: CoverageMask()
			{
				SetBit(mask, sampleId);
			}
			inline void SetBit(int i)
			{
				int id = i >> 5;
				int shift = i & 31;
				bits[id] |= (1 << shift);
			}
			inline void SetBit(const BoolSSE& mask, uint sampleId)
			{
				uint sampleOffset = sampleId << 2;
				if (mask[0] != 0)
				{
					SetBit(sampleOffset + 0);
				}
				if (mask[1] != 0)
				{
					SetBit(sampleOffset + 1);
				}
				if (mask[2] != 0)
				{
					SetBit(sampleOffset + 2);
				}
				if (mask[3] != 0)
				{
					SetBit(sampleOffset + 3);
				}
			}
			inline int GetBit(int i) const
			{
				int id = i >> 5;
				int shift = i & 31;
				return bits[id] & (1 << shift);
			}
			inline int Merge() const
			{
				return bits[0] | bits[1] | bits[2] | bits[3];
			}
		};

		struct Fragment
		{
			FloatSSE lambda0, lambda1;
			CoverageMask coverageMask;

			unsigned short x, y;
			uint vId0, vId1, vId2;
			uint textureId;
			uint tileId;
			uint intraTileIdx;

			Fragment(const FloatSSE& l0,
				const FloatSSE& l1,
				const int id0,
				const int id1,
				const int id2,
				const int texId,
				const Vector2i& pixelCoord,
				const CoverageMask& mask,
				const int tId,
				const uint intraTId)
				: lambda0(l0)
				, lambda1(l1)
				, vId0(id0)
				, vId1(id1)
				, vId2(id2)
				, textureId(texId)
				, x(pixelCoord.x)
				, y(pixelCoord.y)
				, coverageMask(mask)
				, tileId(tId)
				, intraTileIdx(intraTId)
			{
			}

			void Interpolate(const ProjectedVertex& v0,
				const ProjectedVertex& v1,
				const ProjectedVertex& v2,
				FloatSSE& b0,
				FloatSSE& b1,
				Vec3f_SSE& position,
				Vec3f_SSE& normal,
				Vec2f_SSE& texCoord)
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

				position = b0 * Vec3f_SSE(v0.position.x, v0.position.y, v0.position.z) +
					b1 * Vec3f_SSE(v1.position.x, v1.position.y, v1.position.z) +
					b2 * Vec3f_SSE(v2.position.x, v2.position.y, v2.position.z);
				normal = b0 * Vec3f_SSE(v0.normal.x, v0.normal.y, v0.normal.z) +
					b1 * Vec3f_SSE(v1.normal.x, v1.normal.y, v1.normal.z) +
					b2 * Vec3f_SSE(v2.normal.x, v2.normal.y, v2.normal.z);
				texCoord = b0 * Vec2f_SSE(v0.texCoord.x, v0.texCoord.y) +
					b1 * Vec2f_SSE(v1.texCoord.x, v1.texCoord.y) +
					b2 * Vec2f_SSE(v2.texCoord.x, v2.texCoord.y);
			}
		};

		class PixelShader
		{
		public:
			virtual ~PixelShader() {}
			virtual Vec3f_SSE Shade(Fragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				const Vec3f_SSE& position,
				const Vec3f_SSE& normal,
				const Vec2f_SSE& texCoord) const = 0;
		};

		class LambertianPixelShader : public PixelShader
		{
		public:
			Vec3f_SSE Shade(Fragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				const Vec3f_SSE& position,
				const Vec3f_SSE& normal,
				const Vec2f_SSE& texCoord,
				RenderStates& state) const
			{
				FloatSSE w = SSE::Rsqrt(Math::Dot(normal, normal));
				Vec3f_SSE _normal = normal * w;
				Vec3f_SSE vecLightDir = Vec3f_SSE(Math::Normalize(lightDir));

				FloatSSE diffuseAmount = Math::Dot(vecLightDir, _normal);
				BoolSSE mask = diffuseAmount < FloatSSE(Math::EDX_ZERO);
				diffuseAmount = SSE::Select(mask, FloatSSE(Math::EDX_ZERO), diffuseAmount);
				FloatSSE diffuse = (diffuseAmount + 0.2f) * 3 * Math::EDX_INV_PI;

				return diffuse;
			}
		};

		class LambertianAlbedoPixelShader : public PixelShader
		{
		public:
			Vec3f_SSE Shade(Fragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				const Vec3f_SSE& position,
				const Vec3f_SSE& normal,
				const Vec2f_SSE& texCoord) const
			{
				FloatSSE w = SSE::Rsqrt(Math::Dot(normal, normal));
				Vec3f_SSE _normal = normal * w;
				Vec3f_SSE vecLightDir = Vec3f_SSE(Math::Normalize(lightDir));

				FloatSSE diffuseAmount = Math::Dot(vecLightDir, _normal);
				BoolSSE mask = diffuseAmount < FloatSSE(Math::EDX_ZERO);
				diffuseAmount = SSE::Select(mask, FloatSSE(Math::EDX_ZERO), diffuseAmount);

				// Sample texture
				const Vector2 differentials[2] = { Vector2(texCoord.u[1] - texCoord.u[0], texCoord.v[1] - texCoord.v[0]),
					Vector2(texCoord.u[2] - texCoord.u[0], texCoord.v[2] - texCoord.v[0]) };

				RenderStates::Instance()->TextureSlots[fragIn.textureId]->SetFilter(RenderStates::Instance()->GetTextureFilter());
				Vec3f_SSE Albedo;
				for (auto i = 0; i < 4; i++)
				{
					Color color = RenderStates::Instance()->TextureSlots[fragIn.textureId]->Sample(Vector2(texCoord.u[i], texCoord.v[i]), differentials);
					Albedo.x[i] = color.r;
					Albedo.y[i] = color.g;
					Albedo.z[i] = color.b;
				}
				FloatSSE diffuse = (diffuseAmount + 0.2f) * 3 * Math::EDX_INV_PI;

				return diffuse * Albedo;
			}
		};

		class BlinnPhongPixelShader : public PixelShader
		{
		public:
			Vec3f_SSE Shade(Fragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir,
				const Vec3f_SSE& position,
				const Vec3f_SSE& normal,
				const Vec2f_SSE& texCoord) const
			{
				FloatSSE w = SSE::Rsqrt(Math::Dot(normal, normal));
				Vec3f_SSE _normal = normal * w;
				Vec3f_SSE vecLightDir = Vec3f_SSE(Math::Normalize(lightDir));

				FloatSSE diffuseAmount = Math::Dot(vecLightDir, _normal);
				BoolSSE mask = diffuseAmount < FloatSSE(Math::EDX_ZERO);
				diffuseAmount = SSE::Select(mask, FloatSSE(Math::EDX_ZERO), diffuseAmount);

				FloatSSE diffuse = (diffuseAmount + 0.2f) * 3 * Math::EDX_INV_PI;

				Vec3f_SSE eyeDir = Vec3f_SSE(eyePos) - position;
				w = SSE::Rsqrt(Math::Dot(eyeDir, eyeDir));
				eyeDir *= w;

				Vec3f_SSE halfVec = vecLightDir + eyeDir;
				w = SSE::Rsqrt(Math::Dot(halfVec, halfVec));
				halfVec *= w;

				FloatSSE specularAmount = Math::Dot(_normal, halfVec);
				specularAmount = FloatSSE(Math::Pow(specularAmount[0], 200.0f),
					Math::Pow(specularAmount[1], 200.0f),
					Math::Pow(specularAmount[2], 200.0f),
					Math::Pow(specularAmount[3], 200.0f)) * 3.0f;

				return diffuse + specularAmount;
			}
		};
	}
}