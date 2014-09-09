#pragma once

#include "EDXPrerequisites.h"
#include "FrameBuffer.h"
#include "Tile.h"
#include "Shader.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Rasterizer
		{
		private:
			FrameBuffer* mpFrameBuffer;
			vector<ProjectedVertex> mProjectedVertexBuf;

		public:
			virtual ~Rasterizer()
			{
			}

			void CoarseRasterize(Tile& tile,
				const uint tileIdx,
				const Tile::TriangleRef& triRef,
				const uint blockSize,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri);

			__forceinline void FineRasterize(Tile& tile,
				const uint tileIdx,
				const Tile::TriangleRef& triRef,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri)
			{
				static const Vec2i_SSE centerOffset = Vec2i_SSE(IntSSE(8, 24, 8, 24), IntSSE(8, 8, 24, 24));
				const uint sampleCount = mpFrameBuffer->GetSampleCount();
				const uint multiSampleLevel = mpFrameBuffer->GetMultiSampleLevel();

				int minX = Math::Max(blockMin.x, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) >> 4);
				int maxX = Math::Min(blockMax.x - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) >> 4);
				int minY = Math::Max(blockMin.y, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) >> 4);
				int maxY = Math::Min(blockMax.y - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) >> 4);
				minX -= minX % 2;
				minY -= minY % 2;

				if (maxX < minX || maxY < minY)
					return;

				TriangleSSE triSSE;
				triSSE.Load(tri);

				Vec2i_SSE pixelBase = Vec2i_SSE(minX << 4, minY << 4);
				Vec2i_SSE pixelCenter = pixelBase + centerOffset;
				IntSSE edgeVal0 = triRef.acceptEdge0 ? Math::EDX_INFINITY : triSSE.EdgeFunc0(pixelCenter);
				IntSSE edgeVal1 = triRef.acceptEdge1 ? Math::EDX_INFINITY : triSSE.EdgeFunc1(pixelCenter);
				IntSSE edgeVal2 = triRef.acceptEdge2 ? Math::EDX_INFINITY : triSSE.EdgeFunc2(pixelCenter);

				Vector2i pixelCrd;
				for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2)
				{
					IntSSE edgeYBase0 = edgeVal0;
					IntSSE edgeYBase1 = edgeVal1;
					IntSSE edgeYBase2 = edgeVal2;

					for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2)
					{
						pixelCenter = Vec2i_SSE(pixelCrd.x << 4, pixelCrd.y << 4) + centerOffset;
						bool genFragment = false;
						CoverageMask mask;
						for (auto sampleId = 0; sampleId < sampleCount; sampleId++)
						{
							const Vector2i& sampleOffset = FrameBuffer::MultiSampleOffsets[multiSampleLevel][2 * sampleId];
							BoolSSE covered = BoolSSE(Constants::EDX_TRUE);

							if (sampleCount == 1)
							{
								covered = (edgeVal0 | edgeVal1 | edgeVal2) >= IntSSE(Math::EDX_ZERO);
							}
							else
							{
								IntSSE e0 = triRef.acceptEdge0 ? Math::EDX_INFINITY : edgeVal0 + sampleOffset.x * triSSE.B0 + sampleOffset.y * triSSE.C0;
								IntSSE e1 = triRef.acceptEdge1 ? Math::EDX_INFINITY : edgeVal1 + sampleOffset.x * triSSE.B1 + sampleOffset.y * triSSE.C1;
								IntSSE e2 = triRef.acceptEdge2 ? Math::EDX_INFINITY : edgeVal2 + sampleOffset.x * triSSE.B2 + sampleOffset.y * triSSE.C2;

								covered = (e0 | e1 | e2) >= IntSSE(Math::EDX_ZERO);
							}

							if (SSE::Any(covered))
							{
								Vec2i_SSE samplePos = pixelCenter + sampleOffset;
								triSSE.CalcBarycentricCoord(samplePos.x, samplePos.y);

								const ProjectedVertex& v0 = mProjectedVertexBuf[triSSE.vId0];
								const ProjectedVertex& v1 = mProjectedVertexBuf[triSSE.vId1];
								const ProjectedVertex& v2 = mProjectedVertexBuf[triSSE.vId2];

								BoolSSE zTest = mpFrameBuffer->ZTestQuad(triSSE.GetDepth(v0, v1, v2), pixelCrd.x, pixelCrd.y, sampleId, covered);
								BoolSSE visible = zTest & covered;
								if (SSE::Any(visible))
								{
									mask.SetBit(visible, sampleId);
									genFragment = true;
								}
							}
						}

						if (genFragment)
						{
							triSSE.CalcBarycentricCoord(pixelCenter.x, pixelCenter.y);

							QuadFragment frag;
							frag.vId0 = triSSE.vId0;
							frag.vId1 = triSSE.vId1;
							frag.vId2 = triSSE.vId2;
							frag.textureId = triSSE.textureId;
							frag.lambda0 = triSSE.lambda0;
							frag.lambda1 = triSSE.lambda1;
							frag.x = pixelCrd.x;
							frag.y = pixelCrd.y;
							frag.coverageMask = mask;

							frag.tileId = tileIdx;
							frag.intraTileIdx = tile.fragmentBuf.size();

							tile.fragmentBuf.push_back(frag);
						}

						if (!triRef.acceptEdge0)
							edgeVal0 += triSSE.stepB0;
						if (!triRef.acceptEdge1)
							edgeVal1 += triSSE.stepB1;
						if (!triRef.acceptEdge2)
							edgeVal2 += triSSE.stepB2;
					}

					if (!triRef.acceptEdge0)
						edgeVal0 = edgeYBase0 + triSSE.stepC0;
					if (!triRef.acceptEdge1)
						edgeVal1 = edgeYBase1 + triSSE.stepC1;
					if (!triRef.acceptEdge2)
						edgeVal2 = edgeYBase2 + triSSE.stepC2;
				}
			}
		};
	}
}