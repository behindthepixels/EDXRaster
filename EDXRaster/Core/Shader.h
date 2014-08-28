#pragma once

#include "RendererState.h"
#include "Math/Vector.h"

namespace EDX
{
	namespace RasterRenderer
	{
		struct ProjectedVertex
		{
			Vector4 projectedPos;
		};

		class VertexShader
		{
		public:
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
			}
		};
	}
}