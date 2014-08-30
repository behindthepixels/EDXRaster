#pragma once

#include "EDXPrerequisites.h"
#include "Shader.h"
#include "../Utils/InputBuffer.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Clipper
		{
		private:
			static const uint INSIDE_BIT = 0;
			static const uint LEFT_BIT = 1 << 0;
			static const uint RIGHT_BIT = 1 << 1;
			static const uint BOTTOM_BIT = 1 << 2;
			static const uint TOP_BIT = 1 << 3;
			static const uint NEAR_BIT = 1 << 4;
			static const uint FAR_BIT = 1 << 5;

			static uint ComputeClipCode(const Vector4& v)
			{
				uint code = INSIDE_BIT;

				if (v.x < -v.w)
					code |= LEFT_BIT;
				if (v.x > v.w)
					code |= RIGHT_BIT;
				if (v.y < -v.w)
					code |= BOTTOM_BIT;
				if (v.y > v.w)
					code |= TOP_BIT;
				if (v.z < -v.w)
					code |= NEAR_BIT;
				if (v.z > v.w)
					code |= FAR_BIT;

				return code;
			}

		public:
			static void Clip(vector<ProjectedVertex>& verticesIn, IndexBuffer* pIndexBuf)
			{
				for (auto i = 0; i < pIndexBuf->GetTriangleCount(); i++)
				{
					const uint* pIndex = pIndexBuf->GetIndex(i);
					const Vector4& v0 = verticesIn[pIndex[0]].projectedPos;
					const Vector4& v1 = verticesIn[pIndex[1]].projectedPos;
					const Vector4& v2 = verticesIn[pIndex[2]].projectedPos;

					uint clipCode0 = ComputeClipCode(v0);
					uint clipCode1 = ComputeClipCode(v1);
					uint clipCode2 = ComputeClipCode(v2);

					if (!(clipCode0 | clipCode1 | clipCode2))
						continue;

					if (!(clipCode0 & clipCode1 & clipCode2))
						continue;
				}
			}
		};
	}
}