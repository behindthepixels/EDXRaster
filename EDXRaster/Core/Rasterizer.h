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
			vector<ProjectedVertex>& mProjectedVertexBuf;
			const Vec2i_SSE mCenterOffset;

		public:
			Rasterizer(FrameBuffer* pFB, vector<ProjectedVertex>& vb)
				: mpFrameBuffer(pFB)
				, mProjectedVertexBuf(vb)
				, mCenterOffset(Vec2i_SSE(IntSSE(8, 24, 8, 24), IntSSE(8, 8, 24, 24)))
			{
			}

			virtual ~Rasterizer()
			{
			}


			void CoarseRasterize(Tile& tile,
				const Tile::TriangleRef& triRef,
				const uint blockSize,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri)
			{
				const Vector2i blockBase = Vector2i(blockMin.x << 4, blockMin.y << 4);

				Vec3i_SSE rejStepVector, acceptStepVector;
				tri.GenStepVectors(blockSize << 3, &rejStepVector, &acceptStepVector);

				const Vector2i rejCornerOffset0 = Vector2i(tri.rejectCorner0 % 2, tri.rejectCorner0 >> 1) * ((blockSize << 4) - 1);
				IntSSE rejEdgeFunc0 = tri.EdgeFunc0(blockBase + rejCornerOffset0);
				rejEdgeFunc0 += rejStepVector.x;

				const Vector2i rejCornerOffset1 = Vector2i(tri.rejectCorner1 % 2, tri.rejectCorner1 >> 1) * ((blockSize << 4) - 1);
				IntSSE rejEdgeFunc1 = tri.EdgeFunc1(blockBase + rejCornerOffset1);
				rejEdgeFunc1 += rejStepVector.y;

				const Vector2i rejCornerOffset2 = Vector2i(tri.rejectCorner2 % 2, tri.rejectCorner2 >> 1) * ((blockSize << 4) - 1);
				IntSSE rejEdgeFunc2 = tri.EdgeFunc2(blockBase + rejCornerOffset2);
				rejEdgeFunc2 += rejStepVector.z;

				IntSSE acptEdgeFunc0 = Math::EDX_INFINITY;
				if (!triRef.acceptEdge0)
				{
					const Vector2i acptCornerOffset0 = Vector2i(tri.acceptCorner0 % 2, tri.acceptCorner0 >> 1) * ((blockSize << 4) - 1);
					acptEdgeFunc0 = tri.EdgeFunc0(blockBase + acptCornerOffset0);
					acptEdgeFunc0 += acceptStepVector.x;
				}

				IntSSE acptEdgeFunc1 = Math::EDX_INFINITY;
				if (!triRef.acceptEdge1)
				{
					const Vector2i acptCornerOffset1 = Vector2i(tri.acceptCorner1 % 2, tri.acceptCorner1 >> 1) * ((blockSize << 4) - 1);
					acptEdgeFunc1 = tri.EdgeFunc1(blockBase + acptCornerOffset1);
					acptEdgeFunc1 += acceptStepVector.y;
				}

				IntSSE acptEdgeFunc2 = Math::EDX_INFINITY;
				if (!triRef.acceptEdge2)
				{
					const Vector2i acptCornerOffset2 = Vector2i(tri.acceptCorner2 % 2, tri.acceptCorner2 >> 1) * ((blockSize << 4) - 1);
					acptEdgeFunc2 = tri.EdgeFunc2(blockBase + acptCornerOffset2);
					acptEdgeFunc2 += acceptStepVector.z;
				}

				BoolSSE trivialRejectMask = (rejEdgeFunc0 < IntSSE(Math::EDX_ZERO)) |
					(rejEdgeFunc1 < IntSSE(Math::EDX_ZERO)) |
					(rejEdgeFunc2 < IntSSE(Math::EDX_ZERO));
				BoolSSE trivialAcceptMask = (acptEdgeFunc0 >= IntSSE(Math::EDX_ZERO)) &
					(acptEdgeFunc1 >= IntSSE(Math::EDX_ZERO)) &
					(acptEdgeFunc2 >= IntSSE(Math::EDX_ZERO));

				for (auto i = 0; i < 4; i++)
				{
					if (trivialRejectMask[i] != 0)
						continue;

					int minX = !(i % 2) ? blockMin.x : blockMin.x + (blockSize >> 1);
					int maxX = !(i % 2) ? blockMin.x + (blockSize >> 1) : blockMax.x;
					int minY = !(i >> 1) ? blockMin.y : blockMin.y + (blockSize >> 1);
					int maxY = !(i >> 1) ? blockMin.y + (blockSize >> 1) : blockMax.y;

					if (trivialAcceptMask[i] != 0)
					{
						if (mpFrameBuffer->GetSampleCount() == 1)
							TrivialAcceptTriangle_SingleSample(tile, Vector2i(minX, minY), Vector2i(maxX, maxY), tri);
						else
							TrivialAcceptTriangle_MultiSample(tile, Vector2i(minX, minY), Vector2i(maxX, maxY), tri);
						continue;
					}

					if (mpFrameBuffer->GetSampleCount() == 1)
						FineRasterize_SingleSample(tile, triRef, Vector2i(minX, minY), Vector2i(maxX, maxY), tri);
					else
						FineRasterize_MultiSample(tile, triRef, Vector2i(minX, minY), Vector2i(maxX, maxY), tri);
				}
			}

			__forceinline void FineRasterize(Tile& tile,
				const Tile::TriangleRef& triRef,
				const uint blockSize,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri)
			{
				if (mpFrameBuffer->GetSampleCount() == 1)
					FineRasterize_SingleSample(tile, triRef, blockMin, blockMax, tri);
				else
					FineRasterize_MultiSample(tile, triRef, blockMin, blockMax, tri);
			}

			__forceinline void FineRasterize_SingleSample(Tile& tile,
				const Tile::TriangleRef& triRef,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri)
			{
				int minX = Math::Max(blockMin.x, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) >> 4);
				int maxX = Math::Min(blockMax.x - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) >> 4);
				int minY = Math::Max(blockMin.y, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) >> 4);
				int maxY = Math::Min(blockMax.y - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) >> 4);
				minX -= minX % 2;
				minY -= minY % 2;

				if (maxX < minX || maxY < minY)
					return;

				TriangleSSE triSSE(tri);

				Vec2i_SSE pixelBase = Vec2i_SSE(minX << 4, minY << 4);
				Vec2i_SSE pixelCenter = pixelBase + mCenterOffset;
				IntSSE edgeVal0 = triSSE.EdgeFunc0(pixelCenter);
				IntSSE edgeVal1 = triSSE.EdgeFunc1(pixelCenter);
				IntSSE edgeVal2 = triSSE.EdgeFunc2(pixelCenter);

				Vector2i pixelCrd;
				IntSSE pixelBaseStep = IntSSE(32);
				for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2, pixelBase.y += pixelBaseStep)
				{
					IntSSE edgeYBase0 = edgeVal0;
					IntSSE edgeYBase1 = edgeVal1;
					IntSSE edgeYBase2 = edgeVal2;

					pixelBase.x = IntSSE(minX << 4);
					for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2, pixelBase.x += pixelBaseStep)
					{
						pixelCenter = pixelBase + mCenterOffset;
						BoolSSE covered = (edgeVal0 | edgeVal1 | edgeVal2) >= IntSSE(Math::EDX_ZERO);

						if (SSE::Any(covered))
						{
							triSSE.CalcBarycentricCoord(pixelCenter.x, pixelCenter.y);

							const ProjectedVertex& v0 = mProjectedVertexBuf[triSSE.vId0];
							const ProjectedVertex& v1 = mProjectedVertexBuf[triSSE.vId1];
							const ProjectedVertex& v2 = mProjectedVertexBuf[triSSE.vId2];

							BoolSSE zTest = mpFrameBuffer->ZTestQuad(triSSE.GetDepth(v0, v1, v2), pixelCrd.x, pixelCrd.y, 0, covered);
							BoolSSE visible = zTest & covered;
							if (SSE::Any(visible))
							{
								tile.fragmentBuf.push_back(Fragment(triSSE.lambda0,
									triSSE.lambda1,
									triSSE.vId0,
									triSSE.vId1,
									triSSE.vId2,
									triSSE.textureId,
									pixelCrd,
									CoverageMask(visible, 0),
									tile.tileId,
									tile.fragmentBuf.size()));
							}
						}

						edgeVal0 += triSSE.stepB0;
						edgeVal1 += triSSE.stepB1;
						edgeVal2 += triSSE.stepB2;

					}

					edgeVal0 = edgeYBase0 + triSSE.stepC0;
					edgeVal1 = edgeYBase1 + triSSE.stepC1;
					edgeVal2 = edgeYBase2 + triSSE.stepC2;
				}
			}

			__forceinline void FineRasterize_MultiSample(Tile& tile,
				const Tile::TriangleRef& triRef,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri)
			{
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

				TriangleSSE triSSE(tri);

				Vec2i_SSE pixelBase = Vec2i_SSE(minX << 4, minY << 4);
				Vec2i_SSE pixelCenter = pixelBase + mCenterOffset;
				IntSSE edgeVal0 = triSSE.EdgeFunc0(pixelCenter);
				IntSSE edgeVal1 = triSSE.EdgeFunc1(pixelCenter);
				IntSSE edgeVal2 = triSSE.EdgeFunc2(pixelCenter);

				Vector2i pixelCrd;
				IntSSE pixelBaseStep = IntSSE(32);
				for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2, pixelBase.y += pixelBaseStep)
				{
					IntSSE edgeYBase0 = edgeVal0;
					IntSSE edgeYBase1 = edgeVal1;
					IntSSE edgeYBase2 = edgeVal2;

					pixelBase.x = IntSSE(minX << 4);
					for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2, pixelBase.x += pixelBaseStep)
					{
						pixelCenter = pixelBase + mCenterOffset;
						bool genFragment = false;
						CoverageMask mask;
						BoolSSE covered = BoolSSE(Constants::EDX_TRUE);

						for (auto sampleId = 0; sampleId < sampleCount; sampleId++)
						{
							const Vector2i& sampleOffset = FrameBuffer::MultiSampleOffsets[multiSampleLevel][2 * sampleId];
							IntSSE e0 = edgeVal0 + sampleOffset.x * triSSE.B0 + sampleOffset.y * triSSE.C0;
							IntSSE e1 = edgeVal1 + sampleOffset.x * triSSE.B1 + sampleOffset.y * triSSE.C1;
							IntSSE e2 = edgeVal2 + sampleOffset.x * triSSE.B2 + sampleOffset.y * triSSE.C2;

							covered = (e0 | e1 | e2) >= IntSSE(Math::EDX_ZERO);

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

							tile.fragmentBuf.push_back(Fragment(triSSE.lambda0,
								triSSE.lambda1,
								triSSE.vId0,
								triSSE.vId1,
								triSSE.vId2,
								triSSE.textureId,
								pixelCrd,
								mask,
								tile.tileId,
								tile.fragmentBuf.size()));
						}

						edgeVal0 += triSSE.stepB0;
						edgeVal1 += triSSE.stepB1;
						edgeVal2 += triSSE.stepB2;

					}

					edgeVal0 = edgeYBase0 + triSSE.stepC0;
					edgeVal1 = edgeYBase1 + triSSE.stepC1;
					edgeVal2 = edgeYBase2 + triSSE.stepC2;
				}
			}

			__forceinline void TrivialAcceptTriangle(Tile& tile, const Vector2i& blockMin, const Vector2i & blockMax, const RasterTriangle& tri)
			{
				if (mpFrameBuffer->GetSampleCount() == 1)
					TrivialAcceptTriangle_SingleSample(tile, blockMin, blockMax, tri);
				else
					TrivialAcceptTriangle_MultiSample(tile, blockMin, blockMax, tri);
			}

			__forceinline void TrivialAcceptTriangle_SingleSample(Tile& tile, const Vector2i& blockMin, const Vector2i & blockMax, const RasterTriangle& tri)
			{
				int minX = blockMin.x;
				int maxX = blockMax.x - 1;
				int minY = blockMin.y;
				int maxY = blockMax.y - 1;
				minX -= minX % 2;
				minY -= minY % 2;

				TriangleSSE triSSE(tri);

				Vector2i pixelCrd;
				IntSSE pixelBaseStep = IntSSE(32);
				Vec2i_SSE pixelBase = Vec2f_SSE(minX << 4, minY << 4);
				for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2, pixelBase.y += pixelBaseStep)
				{
					pixelBase.x = IntSSE(minX << 4);
					for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2, pixelBase.x += pixelBaseStep)
					{
						Vec2i_SSE pixelCenter = pixelBase + mCenterOffset;
						triSSE.CalcBarycentricCoord(pixelCenter.x, pixelCenter.y);

						const ProjectedVertex& v0 = mProjectedVertexBuf[triSSE.vId0];
						const ProjectedVertex& v1 = mProjectedVertexBuf[triSSE.vId1];
						const ProjectedVertex& v2 = mProjectedVertexBuf[triSSE.vId2];

						BoolSSE zTest = mpFrameBuffer->ZTestQuad(triSSE.GetDepth(v0, v1, v2), pixelCrd.x, pixelCrd.y, 0, BoolSSE(Constants::EDX_TRUE));
						if (SSE::Any(zTest))
						{
							tile.fragmentBuf.push_back(Fragment(triSSE.lambda0,
								triSSE.lambda1,
								triSSE.vId0,
								triSSE.vId1,
								triSSE.vId2,
								triSSE.textureId,
								pixelCrd,
								CoverageMask(zTest, 0),
								tile.tileId,
								tile.fragmentBuf.size()));
						}
					}
				}
			}

			__forceinline void TrivialAcceptTriangle_MultiSample(Tile& tile, const Vector2i& blockMin, const Vector2i & blockMax, const RasterTriangle& tri)
			{
				const uint sampleCount = mpFrameBuffer->GetSampleCount();
				const uint multiSampleLevel = mpFrameBuffer->GetMultiSampleLevel();

				int minX = blockMin.x;
				int maxX = blockMax.x - 1;
				int minY = blockMin.y;
				int maxY = blockMax.y - 1;
				minX -= minX % 2;
				minY -= minY % 2;

				TriangleSSE triSSE(tri);

				Vector2i pixelCrd;
				IntSSE pixelBaseStep = IntSSE(32);
				Vec2i_SSE pixelBase = Vec2f_SSE(minX << 4, minY << 4);
				for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2, pixelBase.y += pixelBaseStep)
				{
					pixelBase.x = IntSSE(minX << 4);
					for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2, pixelBase.x += pixelBaseStep)
					{
						CoverageMask mask;
						bool genFragment = false;
						Vec2i_SSE pixelCenter = pixelBase + mCenterOffset;
						for (auto sampleId = 0; sampleId < sampleCount; sampleId++)
						{
							const Vector2i& sampleOffset = FrameBuffer::MultiSampleOffsets[multiSampleLevel][2 * sampleId];
							Vec2i_SSE samplePos = pixelCenter + sampleOffset;
							triSSE.CalcBarycentricCoord(samplePos.x, samplePos.y);

							const ProjectedVertex& v0 = mProjectedVertexBuf[triSSE.vId0];
							const ProjectedVertex& v1 = mProjectedVertexBuf[triSSE.vId1];
							const ProjectedVertex& v2 = mProjectedVertexBuf[triSSE.vId2];

							BoolSSE zTest = mpFrameBuffer->ZTestQuad(triSSE.GetDepth(v0, v1, v2), pixelCrd.x, pixelCrd.y, sampleId, BoolSSE(Constants::EDX_TRUE));
							if (SSE::Any(zTest))
							{
								mask.SetBit(zTest, sampleId);
								genFragment = true;
							}
						}

						if (genFragment)
						{
							triSSE.CalcBarycentricCoord(pixelCenter.x, pixelCenter.y);
							tile.fragmentBuf.push_back(Fragment(triSSE.lambda0,
								triSSE.lambda1,
								triSSE.vId0,
								triSSE.vId1,
								triSSE.vId2,
								triSSE.textureId,
								pixelCrd,
								mask,
								tile.tileId,
								tile.fragmentBuf.size()));
						}
					}
				}
			}
		};
	}
}