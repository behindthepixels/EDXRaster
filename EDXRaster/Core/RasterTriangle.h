#pragma once

#include "EDXPrerequisites.h"
#include "Math/Vector.h"

namespace EDX
{
	namespace RasterRenderer
	{
		struct RasterTriangle
		{
			Vector2i v0, v1, v2;
			int B0, C0, B1, C1, B2, C2;

			float invDet;
			int triId;

			float lambda0, lambda1; // Barycentric coordinates

			uint vId0, vId1, vId2;

			bool Setup(Vector3& a, Vector3& b, Vector3& c, const uint* pIdx, const Matrix& rasterMatrix)
			{
				a = Matrix::TransformPoint(a, rasterMatrix);
				b = Matrix::TransformPoint(b, rasterMatrix);
				c = Matrix::TransformPoint(c, rasterMatrix);

				// Convert to fixed point
				v0.x = a.x * 16.0;
				v0.y = a.y * 16.0;
				v1.x = b.x * 16.0;
				v1.y = b.y * 16.0;
				v2.x = c.x * 16.0;
				v2.y = c.y * 16.0;

				B0 = v1.y - v0.y;
				C0 = v0.x - v1.x;
				B1 = v2.y - v1.y;
				C1 = v1.x - v2.x;
				B2 = v0.y - v2.y;
				C2 = v2.x - v0.x;

				int det = C2 * B1 - C1 * B2;
				if (det <= 0)
					return false;

				invDet = 1.0f / float(Math::Abs(det));
				vId0 = pIdx[0]; vId1 = pIdx[1]; vId2 = pIdx[2];

				return true;
			}

			int TopLeftEdge(const Vector2i& v1, const Vector2i& v2) const
			{
				return ((v2.y > v1.y) || (v1.y == v2.y && v1.x > v2.x)) ? 0 : 1;
			}

			bool Inside(const Vector2i& p) const
			{
				return B0 * (p.x - v0.x) + C0 * (p.y - v0.y) + TopLeftEdge(v0, v1) <= 0 &&
					B1 * (p.x - v1.x) + C1 * (p.y - v1.y) + TopLeftEdge(v1, v2) <= 0 &&
					B2 * (p.x - v2.x) + C2 * (p.y - v2.y) + TopLeftEdge(v2, v0) <= 0;
			}

			void CalcBarycentricCoord(const int x, const int y)
			{
				lambda0 = (B1 * (v2.x - x) + C1 * (v2.y - y)) * invDet;
				lambda1 = (B2 * (v2.x - x) + C2 * (v2.y - y)) * invDet;
			}
		};
	}
}