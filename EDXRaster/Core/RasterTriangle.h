#pragma once

#include "EDXPrerequisites.h"
#include "Math/Vector.h"
#include "SIMD/SSE.h"

namespace EDX
{
	namespace RasterRenderer
	{
		struct RasterTriangle
		{
			Vector2i v0, v1, v2;
			int B0, C0, B1, C1, B2, C2;
			int stepB0, stepC0, stepB1, stepC1, stepB2, stepC2;

			float invDet;
			int vId0 : 24, vId1 : 24, vId2 : 24;
			int textureId : 24;

			float lambda0, lambda1; // Barycentric coordinates


			bool Setup(Vector3& a, Vector3& b, Vector3& c, const uint* pIdx, const uint texId, const Matrix& rasterMatrix)
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

				B0 = v0.y - v1.y;
				C0 = v1.x - v0.x;
				B1 = v1.y - v2.y;
				C1 = v2.x - v1.x;
				B2 = v2.y - v0.y;
				C2 = v0.x - v2.x;

				int det = C2 * B1 - C1 * B2;
				if (det <= 0)
					return false;

				stepB0 = 16 * B0;
				stepC0 = 16 * C0;
				stepB1 = 16 * B1;
				stepC1 = 16 * C1;
				stepB2 = 16 * B2;
				stepC2 = 16 * C2;

				invDet = 1.0f / float(Math::Abs(det));
				vId0 = pIdx[0]; vId1 = pIdx[1]; vId2 = pIdx[2];
				textureId = texId;

				return true;
			}

			__forceinline int TopLeftEdge(const Vector2i& v1, const Vector2i& v2) const
			{
				return ((v2.y > v1.y) || (v1.y == v2.y && v1.x > v2.x)) ? 0 : -1;
			}

			__forceinline int EdgeFunc0(const Vector2i& p) const
			{
				return B0 * (p.x - v0.x) + C0 * (p.y - v0.y) + TopLeftEdge(v0, v1);
			}
			__forceinline int EdgeFunc1(const Vector2i& p) const
			{
				return B1 * (p.x - v1.x) + C1 * (p.y - v1.y) + TopLeftEdge(v1, v2);
			}
			__forceinline int EdgeFunc2(const Vector2i& p) const
			{
				return B2 * (p.x - v2.x) + C2 * (p.y - v2.y) + TopLeftEdge(v2, v0);
			}

			__forceinline bool Inside(const Vector2i& p) const
			{
				return ((B0 * (p.x - v0.x) + C0 * (p.y - v0.y) + TopLeftEdge(v0, v1)) |
					(B1 * (p.x - v1.x) + C1 * (p.y - v1.y) + TopLeftEdge(v1, v2)) |
					(B2 * (p.x - v2.x) + C2 * (p.y - v2.y) + TopLeftEdge(v2, v0))) >= 0;
			}

			__forceinline float GetDepth(const ProjectedVertex& v0,
				const ProjectedVertex& v1,
				const ProjectedVertex& v2)
			{
				const float lambda2 = 1.0f - lambda0 - lambda1;
				return lambda0 * v0.projectedPos.z + lambda1 * v1.projectedPos.z + lambda2 * v2.projectedPos.z;
			}

			__forceinline void CalcBarycentricCoord(const int x, const int y)
			{
				lambda0 = (B1 * (x - v2.x) + C1 * (y - v2.y)) * invDet;
				lambda1 = (B2 * (x - v2.x) + C2 * (y - v2.y)) * invDet;
			}
		};

		struct TriangleSIMD
		{
			Vec2i_SSE v0, v1, v2;
			IntSSE B0, C0, B1, C1, B2, C2;

			IntSSE stepB0, stepC0, stepB1, stepC1, stepB2, stepC2;

			FloatSSE invDet;
			uint vId0, vId1, vId2;
			uint textureId;

			FloatSSE lambda0, lambda1;

			__forceinline void LoadCoords(const RasterTriangle& tri)
			{
				v0 = tri.v0;
				v1 = tri.v1;
				v2 = tri.v2;

				B0 = tri.B0;
				C0 = tri.C0;
				B1 = tri.B1;
				C1 = tri.C1;
				B2 = tri.B2;
				C2 = tri.C2;
			}

			void Load(const RasterTriangle& tri)
			{
				LoadCoords(tri);

				invDet = tri.invDet;

				stepB0 = 2 * tri.stepB0;
				stepC0 = 2 * tri.stepC0;
				stepB1 = 2 * tri.stepB1;
				stepC1 = 2 * tri.stepC1;
				stepB2 = 2 * tri.stepB2;
				stepC2 = 2 * tri.stepC2;

				vId0 = tri.vId0;
				vId1 = tri.vId1;
				vId2 = tri.vId2;
				textureId = tri.textureId;
			}

			__forceinline IntSSE TopLeftEdge(const Vec2i_SSE& v1, const Vec2i_SSE& v2) const
			{
				return ((v2.y > v1.y) | ((v1.y == v2.y) & (v1.x > v2.x)));
			}

			__forceinline IntSSE EdgeFunc0(const Vec2i_SSE& p) const
			{
				return B0 * (p.x - v0.x) + C0 * (p.y - v0.y) + TopLeftEdge(v0, v1);
			}
			__forceinline IntSSE EdgeFunc1(const Vec2i_SSE& p) const
			{
				return B1 * (p.x - v1.x) + C1 * (p.y - v1.y) + TopLeftEdge(v1, v2);
			}
			__forceinline IntSSE EdgeFunc2(const Vec2i_SSE& p) const
			{
				return B2 * (p.x - v2.x) + C2 * (p.y - v2.y) + TopLeftEdge(v2, v0);
			}

			__forceinline BoolSSE Inside(const Vec2i_SSE& p) const
			{
				return (EdgeFunc0(p) | EdgeFunc1(p) | EdgeFunc2(p)) >= IntSSE(Math::EDX_ZERO);
			}
			__forceinline bool TrivialReject(const Vec2i_SSE& p) const
			{
				return SSE::Any((EdgeFunc0(p) & EdgeFunc1(p) & EdgeFunc2(p)) < IntSSE(Math::EDX_ZERO));
			}

			__forceinline FloatSSE GetDepth(const ProjectedVertex& v0,
				const ProjectedVertex& v1,
				const ProjectedVertex& v2)
			{
				const auto One = FloatSSE(Math::EDX_ONE);
				const FloatSSE lambda2 = One - lambda0 - lambda1;
				return lambda0 * v0.projectedPos.z + lambda1 * v1.projectedPos.z + lambda2 * v2.projectedPos.z;
			}

			__forceinline void CalcBarycentricCoord(const IntSSE& x, const IntSSE& y)
			{
				lambda0 = FloatSSE((B1 * (x - v2.x) + C1 * (y - v2.y))) * invDet;
				lambda1 = FloatSSE((B2 * (x - v2.x) + C2 * (y - v2.y))) * invDet;
			}
		};
	}
}