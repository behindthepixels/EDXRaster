#pragma once

#include "Shader.h"
#include "Math/Vector.h"

namespace EDX
{
	namespace RasterRenderer
	{
		struct Tile
		{
			static const int SIZE_LOG_2 = 5;
			static const int SIZE = 1 << SIZE_LOG_2;

			struct TriangleRef
			{
				uint triId;
				bool acceptEdge0;
				bool acceptEdge1;
				bool acceptEdge2;
				bool trivialAccept;

				TriangleRef(uint id, bool acptE0 = false, bool acptE1 = false, bool acptE2 = false)
					: triId(id), acceptEdge0(acptE0), acceptEdge1(acptE1), acceptEdge2(acptE2), trivialAccept(false)
				{
					if (acceptEdge0 && acceptEdge1 && acceptEdge2)
						trivialAccept = true;
				}
			};

			Vector2i minCoord, maxCoord;
			vector<TriangleRef> triangleRefs[12];
			vector<QuadFragment> fragmentBuf;

			Tile(const Vector2i& min, const Vector2i& max)
				: minCoord(min), maxCoord(max)
			{
			}
		};
	}
}