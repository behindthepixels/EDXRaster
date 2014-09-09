
#include "Renderer.h"
#include "FrameBuffer.h"
#include "Scene.h"
#include "Clipper.h"
#include "../Utils/Mesh.h"
#include "../Utils/InputBuffer.h"
#include "Math/Matrix.h"

#include <ppl.h>
using namespace concurrency;

namespace EDX
{
	namespace RasterRenderer
	{
		void Renderer::Initialize(uint iScreenWidth, uint iScreenHeight)
		{
			mTileDim.x = (iScreenWidth + Tile::SIZE - 1) >> Tile::SIZE_LOG_2;
			mTileDim.y = (iScreenHeight + Tile::SIZE - 1) >> Tile::SIZE_LOG_2;

			if (!mpFrameBuffer)
			{
				mpFrameBuffer = new FrameBuffer;
			}
			mpFrameBuffer->Init(iScreenWidth, iScreenHeight, mTileDim, 0);

			if (!mpScene)
			{
				mpScene = new Scene;
			}

			mpVertexShader = new DefaultVertexShader;
			mpPixelShader = new QuadLambertianAlbedoPixelShader;

			for (auto i = 0; i < iScreenHeight; i += Tile::SIZE)
			{
				for (auto j = 0; j < iScreenWidth; j += Tile::SIZE)
				{
					auto maxX = Math::Min(j + Tile::SIZE, iScreenWidth);
					auto maxY = Math::Min(i + Tile::SIZE, iScreenHeight);

					mTiles.push_back(Tile(Vector2i(j, i), Vector2i(maxX, maxY)));
				}
			}

			mFrameCount = 0;

			mNumCores = DetectCPUCount();
		}

		void Renderer::SetRenderState(const Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster)
		{
			mGlobalRenderStates.mModelViewMatrix = mModelView;
			mGlobalRenderStates.mModelViewInvMatrix = Matrix::Inverse(mModelView);
			mGlobalRenderStates.mProjMatrix = mProj;
			mGlobalRenderStates.mModelViewProjMatrix = mProj * mModelView;
			mGlobalRenderStates.mRasterMatrix = mToRaster;
		}

		void Renderer::RenderMesh(const Mesh& mesh)
		{
			mFrameCount++;
			mpFrameBuffer->Clear();
			mGlobalRenderStates.mTextureSlots = mesh.GetTextures();

			VertexProcessing(mesh.GetVertexBuffer());
			Clipping(mesh.GetIndexBuffer(), mesh.GetTextureIds());
			TiledRasterization();
			FragmentProcessing();
		}

		void Renderer::VertexProcessing(const IVertexBuffer* pVertexBuf)
		{
			mProjectedVertexBuf.resize(pVertexBuf->GetVertexCount());
			parallel_for(0, (int)pVertexBuf->GetVertexCount(), [&](int i)
			{
				mpVertexShader->Execute(mGlobalRenderStates, pVertexBuf->GetPosition(i), pVertexBuf->GetNormal(i), pVertexBuf->GetTexCoord(i), &mProjectedVertexBuf[i]);
			});
		}

		void Renderer::Clipping(IndexBuffer* pIndexBuf, const vector<uint>& texIdBuf)
		{
			mRasterTriangleBuf.clear();
			Clipper::Clip(mProjectedVertexBuf, pIndexBuf, texIdBuf, mGlobalRenderStates.GetRasterMatrix(), mRasterTriangleBuf);

			parallel_for(0, (int)mProjectedVertexBuf.size(), [&](int i)
			{
				ProjectedVertex& vertex = mProjectedVertexBuf[i];
				vertex.invW = 1.0f / vertex.projectedPos.w;
				vertex.projectedPos.z *= vertex.invW;
			});
		}

		void Renderer::TiledRasterization()
		{
			// Binning triangles
			parallel_for(0, (int)mTiles.size(), [&](int i)
			{
				for (auto c = 0; c < mNumCores; c++)
					mTiles[i].triangleRefs[c].clear();

				mTiles[i].fragmentBuf.clear();
			});

			const int Shift = Tile::SIZE_LOG_2 + 4;
			parallel_for(0, mNumCores, [&](int coreId)
			{
				auto interval = (mRasterTriangleBuf.size() + mNumCores - 1) / mNumCores;
				auto startIdx = coreId * interval;
				auto endIdx = (coreId + 1) * interval;

				for (auto i = startIdx; i < endIdx; i++)
				{
					if (i >= mRasterTriangleBuf.size())
						return;

					const RasterTriangle& tri = mRasterTriangleBuf[i];

					int minX = Math::Max(0, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) >> Shift);
					int maxX = Math::Min(mTileDim.x - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) >> Shift);
					int minY = Math::Max(0, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) >> Shift);
					int maxY = Math::Min(mTileDim.y - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) >> Shift);

					if (maxX - minX <= 2 && maxY - minY <= 2)
					{
						for (auto y = minY; y <= maxY; y++)
						{
							for (auto x = minX; x <= maxX; x++)
							{
								mTiles[y * mTileDim.x + x].triangleRefs[coreId].push_back(Tile::TriangleRef(i));
							}
						}
					}
					else
					{
						for (auto y = minY; y <= maxY; y++)
						{
							for (auto x = minX; x <= maxX; x++)
							{
								Vector2i pixelBase = Vector2i(x, y);

								const Vector2i rejCornerOffset0 = Vector2i(tri.rejectCorner0 % 2, tri.rejectCorner0 / 2);
								const Vector2i rejCornerOffset1 = Vector2i(tri.rejectCorner1 % 2, tri.rejectCorner1 / 2);
								const Vector2i rejCornerOffset2 = Vector2i(tri.rejectCorner2 % 2, tri.rejectCorner2 / 2);

								const Vector2i rejCorner0 = Vector2i((pixelBase.x + rejCornerOffset0.x) << Shift,
									(pixelBase.y + rejCornerOffset0.y) << Shift);
								const Vector2i rejCorner1 = Vector2i((pixelBase.x + rejCornerOffset1.x) << Shift,
									(pixelBase.y + rejCornerOffset1.y) << Shift);
								const Vector2i rejCorner2 = Vector2i((pixelBase.x + rejCornerOffset2.x) << Shift,
									(pixelBase.y + rejCornerOffset2.y) << Shift);

								if (tri.EdgeFunc0(rejCorner0) < 0 || tri.EdgeFunc1(rejCorner1) < 0 || tri.EdgeFunc2(rejCorner2) < 0)
									continue;


								const Vector2i acptCornerOffset0 = Vector2i(tri.acceptCorner0 % 2, tri.acceptCorner0 / 2);
								const Vector2i acptCornerOffset1 = Vector2i(tri.acceptCorner1 % 2, tri.acceptCorner1 / 2);
								const Vector2i acptCornerOffset2 = Vector2i(tri.acceptCorner2 % 2, tri.acceptCorner2 / 2);

								const Vector2i acptCorner0 = Vector2i((pixelBase.x + acptCornerOffset0.x) << Shift,
									(pixelBase.y + acptCornerOffset0.y) << Shift);
								const Vector2i acptCorner1 = Vector2i((pixelBase.x + acptCornerOffset1.x) << Shift,
									(pixelBase.y + acptCornerOffset1.y) << Shift);
								const Vector2i acptCorner2 = Vector2i((pixelBase.x + acptCornerOffset2.x) << Shift,
									(pixelBase.y + acptCornerOffset2.y) << Shift);

								mTiles[y * mTileDim.x + x].triangleRefs[coreId].push_back(Tile::TriangleRef(i,
									tri.EdgeFunc0(acptCorner0) >= 0,
									tri.EdgeFunc1(acptCorner1) >= 0,
									tri.EdgeFunc2(acptCorner2) >= 0));
							}
						}
					}
				}
			});


			//for (auto i = 0; i < mTiles.size(); i++)
			parallel_for(0, (int)mTiles.size(), [&](int i)
			{
				//RasterizeTile(mTiles[i], i);
				RasterizeTile_Hierarchical(mTiles[i], i, Tile::SIZE);
			});

			mFragmentBuf.clear();
			mTiledShadingResultBuf.resize(mTiles.size());
			for (auto i = 0; i < mTiles.size(); i++)
			{
				mTiledShadingResultBuf[i].resize(mTiles[i].fragmentBuf.size());
				mFragmentBuf.insert(mFragmentBuf.end(), mTiles[i].fragmentBuf.begin(), mTiles[i].fragmentBuf.end());
			}
		}

		void Renderer::RasterizeTile(Tile& tile, uint tileIdx)
		{
			const uint sampleCount = mpFrameBuffer->GetSampleCount();
			const uint multiSampleLevel = mpFrameBuffer->GetMultiSampleLevel();

			static const Vec2i_SSE centerOffset = Vec2i_SSE(IntSSE(8, 24, 8, 24), IntSSE(8, 8, 24, 24));

			for (auto coreId = 0; coreId < mNumCores; coreId++)
			{
				for (auto j = 0; j < tile.triangleRefs[coreId].size(); j++)
				{
					const Tile::TriangleRef& triRef = tile.triangleRefs[coreId][j];
					RasterTriangle& tri = mRasterTriangleBuf[triRef.triId];

					if (triRef.trivialAccept)
					{
						TrivialAcceptTriangle(tile, tileIdx, tile.minCoord, tile.maxCoord, tri);
						continue;
					}

					int minX = Math::Max(tile.minCoord.x, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) >> 4);
					int maxX = Math::Min(tile.maxCoord.x - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) >> 4);
					int minY = Math::Max(tile.minCoord.y, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) >> 4);
					int maxY = Math::Min(tile.maxCoord.y - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) >> 4);
					minX -= minX % 2;
					minY -= minY % 2;

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
								BoolSSE covered = BoolSSE(true, true, true, true);

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
			}
		}

		void Renderer::TrivialAcceptTriangle(Tile& tile, uint tileIdx, const Vector2i& blockMin, const Vector2i & blockMax, const RasterTriangle& tri)
		{
			const uint sampleCount = mpFrameBuffer->GetSampleCount();
			const uint multiSampleLevel = mpFrameBuffer->GetMultiSampleLevel();

			static const Vec2i_SSE centerOffset = Vec2i_SSE(IntSSE(8, 24, 8, 24), IntSSE(8, 8, 24, 24));

			int minX = blockMin.x;
			int maxX = blockMax.x - 1;
			int minY = blockMin.y;
			int maxY = blockMax.y - 1;
			minX -= minX % 2;
			minY -= minY % 2;

			TriangleSSE triSSE;
			triSSE.Load(tri);

			Vector2i pixelCrd;
			for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2)
			{
				for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2)
				{
					CoverageMask mask;
					bool genFragment = false;
					Vec2i_SSE pixelCenter = Vec2i_SSE(pixelCrd.x << 4, pixelCrd.y << 4) + centerOffset;

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
				}
			}
		}

		void Renderer::RasterizeTile_Hierarchical(Tile& tile, uint tileIdx, const uint blockSize)
		{
			for (auto coreId = 0; coreId < mNumCores; coreId++)
			{
				for (auto j = 0; j < tile.triangleRefs[coreId].size(); j++)
				{
					const Tile::TriangleRef& triRef = tile.triangleRefs[coreId][j];
					RasterTriangle& tri = mRasterTriangleBuf[triRef.triId];

					if (triRef.trivialAccept)
					{
						TrivialAcceptTriangle(tile, tileIdx, tile.minCoord, tile.maxCoord, tri);
						continue;
					}

					//FineRasterize(tile, tileIdx, triRef, tile.minCoord, tile.maxCoord, tri);
					CoarseRasterize(tile, tileIdx, triRef, blockSize, tile.minCoord, tile.maxCoord, tri);
				}
			}

		}

		void Renderer::CoarseRasterize(Tile& tile,
			const uint tileIdx,
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
					TrivialAcceptTriangle(tile, tileIdx, Vector2i(minX, minY), Vector2i(maxX, maxY), tri);
					continue;
				}

				FineRasterize(tile, tileIdx, triRef, Vector2i(minX, minY), Vector2i(maxX, maxY), tri);
			}
		}

		void Renderer::FineRasterize(Tile& tile,
			const uint tileIdx,
			const Tile::TriangleRef& triRef,
			const Vector2i& blockMin,
			const Vector2i& blockMax,
			const RasterTriangle& tri)
		{
			const Vec2i_SSE centerOffset = Vec2i_SSE(IntSSE(8, 24, 8, 24), IntSSE(8, 8, 24, 24));
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

		void Renderer::FragmentProcessing()
		{
			//for (auto i = 0; i < mFragmentBuf.size(); i++)
			parallel_for(0, (int)mFragmentBuf.size(), [&](int i)
			{
				QuadFragment& frag = mFragmentBuf[i];

				const ProjectedVertex& v0 = mProjectedVertexBuf[frag.vId0];
				const ProjectedVertex& v1 = mProjectedVertexBuf[frag.vId1];
				const ProjectedVertex& v2 = mProjectedVertexBuf[frag.vId2];

				Vec3f_SSE position;
				Vec3f_SSE normal;
				Vec2f_SSE texCoord;
				frag.Interpolate(v0, v1, v2, frag.lambda0, frag.lambda1, position, normal, texCoord);

				mTiledShadingResultBuf[frag.tileId][frag.intraTileIdx] = mpPixelShader->Shade(frag,
					Matrix::TransformPoint(Vector3::ZERO, mGlobalRenderStates.GetModelViewInvMatrix()),
					Vector3(-1, 1, -1),
					position,
					normal,
					texCoord,
					mGlobalRenderStates);
			});

			parallel_for(0, (int)mTiledShadingResultBuf.size(), [&](int i)
			{
				for (auto j = 0; j < mTiles[i].fragmentBuf.size(); j++)
				{
					const QuadFragment& frag = mTiles[i].fragmentBuf[j];
					for (auto sId = 0; sId < mpFrameBuffer->GetSampleCount(); sId++)
					{
						int maskShift = sId << 2;
						if (frag.coverageMask.GetBit(maskShift) != 0)
						{
							mpFrameBuffer->SetPixel(Color(mTiledShadingResultBuf[i][j].x[0],
								mTiledShadingResultBuf[i][j].y[0],
								mTiledShadingResultBuf[i][j].z[0]),
								frag.x, frag.y, sId);
						}
						if (frag.coverageMask.GetBit(maskShift + 1) != 0)
						{
							mpFrameBuffer->SetPixel(Color(mTiledShadingResultBuf[i][j].x[1],
								mTiledShadingResultBuf[i][j].y[1],
								mTiledShadingResultBuf[i][j].z[1]),
								frag.x + 1, frag.y, sId);
						}
						if (frag.coverageMask.GetBit(maskShift + 2) != 0)
						{
							mpFrameBuffer->SetPixel(Color(mTiledShadingResultBuf[i][j].x[2],
								mTiledShadingResultBuf[i][j].y[2],
								mTiledShadingResultBuf[i][j].z[2]),
								frag.x, frag.y + 1, sId);
						}
						if (frag.coverageMask.GetBit(maskShift + 3) != 0)
						{
							mpFrameBuffer->SetPixel(Color(mTiledShadingResultBuf[i][j].x[3],
								mTiledShadingResultBuf[i][j].y[3],
								mTiledShadingResultBuf[i][j].z[3]),
								frag.x + 1, frag.y + 1, sId);
						}
					}
				}
			});

			mpFrameBuffer->Resolve();
		}

		const float* Renderer::GetBackBuffer() const
		{
			return mpFrameBuffer->GetColorBuffer();
		}
	}
}