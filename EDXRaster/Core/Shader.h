#pragma once

#include "Renderer.h"
#include "RendererState.h"
#include "Graphics/ObjMesh.h"
#include "Math/Vector.h"

namespace EDX
{
	namespace RasterRenderer
	{
		static const uint MAX_VARYING = 18;

		struct VertexShaderOutput
		{
			Vector4 projectedPos;
			float varying[MAX_VARYING];
		};

		class VertexShader
		{
		public:
			virtual void Execute(const RendererState& renderState,
				const Vector3& vPosIn,
				const Vector3& vNormalIn,
				const Vector2& vTexIn,
				VertexShaderOutput* pOut) = 0;
		};

		class DefaultVertexShader : public VertexShader
		{
		public:
			virtual void Execute(const RendererState& renderState,
				const Vector3& vPosIn,
				const Vector3& vNormalIn,
				const Vector2& vTexIn,
				VertexShaderOutput* pOut)
			{
				pOut->projectedPos = Matrix::TransformPoint(Vector4(vPosIn.x, vPosIn.y, vPosIn.z, 1.0f), renderState.GetModelViewProjMatrix());
			}
		};
	}
}