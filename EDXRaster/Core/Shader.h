#pragma once

#include "RendererState.h"
#include "Math/Vector.h"
#include "Graphics/Color.h"

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
		};

		class VertexShader
		{
		public:
			virtual ~VertexShader() {}
			virtual void Execute(const RendererState& renderState,
				const Vector3& vPosIn,
				const Vector3& vNormalIn,
				const Vector2& vTexIn,
				ProjectedVertex* pOut) = 0;
		};

		class DefaultVertexShader : public VertexShader
		{
		public:
			virtual void Execute(const RendererState& renderState,
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

			void SetupAndInterpolate(const ProjectedVertex& v0,
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

		class PixelShader
		{
		public:
			virtual ~PixelShader() {}
			virtual Color Shade(const Fragment& fragIn,
				const Vector3& eyePos,
				const Vector3& lightDir) const = 0;
		};

		class BlinnPhonePixelShader : public PixelShader
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
	}
}