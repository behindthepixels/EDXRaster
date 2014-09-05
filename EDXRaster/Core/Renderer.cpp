
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
			if (!mpFrameBuffer)
			{
				mpFrameBuffer = new FrameBuffer;
			}
			mpFrameBuffer->Init(iScreenWidth, iScreenHeight, 2);

			if (!mpScene)
			{
				mpScene = new Scene;
			}

			mpVertexShader = new DefaultVertexShader;
			mpPixelShader = new QuadBlinnPhongPixelShader;

			mTileDim.x = (iScreenWidth + Tile::SIZE - 1) >> Tile::SIZE_LOG_2;
			mTileDim.y = (iScreenHeight + Tile::SIZE - 1) >> Tile::SIZE_LOG_2;
			for (auto i = 0; i < iScreenHeight; i += Tile::SIZE)
			{
				for (auto j = 0; j < iScreenWidth; j += Tile::SIZE)
				{
					auto maxX = Math::Min(j + Tile::SIZE, iScreenWidth);
					auto maxY = Math::Min(i + Tile::SIZE, iScreenHeight);

					mTiles.push_back(Tile(Vector2i(j, i), Vector2i(maxX, maxY)));
				}
			}

			FrameCount = 0;
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
			FrameCount++;
			mpFrameBuffer->Clear();
			mGlobalRenderStates.mTextures = mesh.GetTextures();

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
			for (auto i = 0; i < mTiles.size(); i++)
			{
				mTiles[i].triangleIds.clear();
				mTiles[i].fragmentBuf.clear();
			}

			const int Shift = Tile::SIZE_LOG_2 + 4;
			for (auto i = 0; i < mRasterTriangleBuf.size(); i++)
			{
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
							mTiles[y * mTileDim.x + x].triangleIds.push_back(i);
						}
					}
				}
				else
				{
					for (auto y = minY; y <= maxY; y++)
					{
						IntSSE rasY = IntSSE(y << Shift, y << Shift, (y + 1) << Shift, (y + 1) << Shift);
						for (auto x = minX; x <= maxX; x++)
						{
							IntSSE rasX = IntSSE(x << Shift, (x + 1) << Shift, x << Shift, (x + 1) << Shift);
							Vec2i_SSE p = Vec2i_SSE(rasX, rasY);

							TriangleSIMD triSIMD;
							triSIMD.LoadCoords(tri);
							if (!triSIMD.TrivialReject(p))
							{
								mTiles[y * mTileDim.x + x].triangleIds.push_back(i);
							}
						}
					}
				}
			}

			const uint sampleCount = mpFrameBuffer->GetSampleCount();
			const uint multiSampleLevel = mpFrameBuffer->GetMultiSampleLevel();

			//for (auto i = 0; i < mTiles.size(); i++)
			parallel_for(0, (int)mTiles.size(), [&](int i)
			{
				Tile& tile = mTiles[i];
				for (auto j = 0; j < tile.triangleIds.size(); j++)
				{
					RasterTriangle& tri = mRasterTriangleBuf[tile.triangleIds[j]];

					int minX = Math::Max(tile.minCoord.x, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) >> 4);
					int maxX = Math::Min(tile.maxCoord.x - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) >> 4);
					int minY = Math::Max(tile.minCoord.y, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) >> 4);
					int maxY = Math::Min(tile.maxCoord.y - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) >> 4);
					minX -= minX % 2;
					minY -= minY % 2;

					TriangleSIMD triSIMD;
					triSIMD.Load(tri);

					Vector2i sampleCoord = Vector2i(minX << 4, minY << 4);
					Vec2i_SSE rasterSamplePos = Vec2i_SSE(IntSSE(sampleCoord.x + 8, sampleCoord.x + 24, sampleCoord.x + 8, sampleCoord.x + 24), IntSSE(sampleCoord.y + 8, sampleCoord.y + 8, sampleCoord.y + 24, sampleCoord.y + 24));
					IntSSE edgeVal0 = triSIMD.EdgeFunc0(rasterSamplePos);
					IntSSE edgeVal1 = triSIMD.EdgeFunc1(rasterSamplePos);
					IntSSE edgeVal2 = triSIMD.EdgeFunc2(rasterSamplePos);

					Vector2i pixelCrd;
					for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2)
					{
						IntSSE edgeYBase0 = edgeVal0;
						IntSSE edgeYBase1 = edgeVal1;
						IntSSE edgeYBase2 = edgeVal2;

						for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2)
						{
							const Vector2i sampleCrdBase = Vector2i(pixelCrd.x << 4, pixelCrd.y << 4);
							bool genFragment = false;
							QuadFragment frag;
							for (auto sampleId = 0; sampleId < sampleCount; sampleId++)
							{
								const Vector2i& sampleOffset = FrameBuffer::MultiSampleOffsets[multiSampleLevel][2 * sampleId];
								BoolSSE covered;
								if (sampleCount == 1)
								{
									covered = (edgeVal0 | edgeVal1 | edgeVal2) >= IntSSE(Math::EDX_ZERO);
								}
								else
								{
									IntSSE e0 = edgeVal0 + sampleOffset.x * triSIMD.B0 + sampleOffset.y * triSIMD.C0;
									IntSSE e1 = edgeVal1 + sampleOffset.x * triSIMD.B1 + sampleOffset.y * triSIMD.C1;
									IntSSE e2 = edgeVal2 + sampleOffset.x * triSIMD.B2 + sampleOffset.y * triSIMD.C2;

									covered = (e0 | e1 | e2) >= IntSSE(Math::EDX_ZERO);
								}

								if (SSE::Any(covered))
								{
									sampleCoord = sampleCrdBase + sampleOffset;
									rasterSamplePos = Vec2i_SSE(IntSSE(sampleCoord.x + 8, sampleCoord.x + 24, sampleCoord.x + 8, sampleCoord.x + 24), IntSSE(sampleCoord.y + 8, sampleCoord.y + 8, sampleCoord.y + 24, sampleCoord.y + 24));
									triSIMD.CalcBarycentricCoord(rasterSamplePos.x, rasterSamplePos.y);

									const ProjectedVertex& v0 = mProjectedVertexBuf[triSIMD.vId0];
									const ProjectedVertex& v1 = mProjectedVertexBuf[triSIMD.vId1];
									const ProjectedVertex& v2 = mProjectedVertexBuf[triSIMD.vId2];

									BoolSSE zTest = mpFrameBuffer->ZTestQuad(frag.GetDepth(v0, v1, v2, sampleId, triSIMD.lambda0, triSIMD.lambda1), pixelCrd.x, pixelCrd.y, sampleId, covered);
									BoolSSE visible = zTest & covered;
									if (SSE::Any(visible))
									{
										frag.coverageMask.SetBit(visible, sampleId);
										genFragment = true;
									}
								}
							}

							if (genFragment)
							{
								sampleCoord = sampleCrdBase;
								rasterSamplePos = Vec2i_SSE(IntSSE(sampleCoord.x + 8, sampleCoord.x + 24, sampleCoord.x + 8, sampleCoord.x + 24), IntSSE(sampleCoord.y + 8, sampleCoord.y + 8, sampleCoord.y + 24, sampleCoord.y + 24));
								triSIMD.CalcBarycentricCoord(rasterSamplePos.x, rasterSamplePos.y);

								frag.vId0 = triSIMD.vId0;
								frag.vId1 = triSIMD.vId1;
								frag.vId2 = triSIMD.vId2;
								frag.textureId = triSIMD.textureId;
								frag.lambda0 = triSIMD.lambda0;
								frag.lambda1 = triSIMD.lambda1;
								frag.x = pixelCrd.x;
								frag.y = pixelCrd.y;

								tile.fragmentBuf.push_back(frag);
							}

							edgeVal0 += triSIMD.stepB0;
							edgeVal1 += triSIMD.stepB1;
							edgeVal2 += triSIMD.stepB2;
						}

						edgeVal0 = edgeYBase0 + triSIMD.stepC0;
						edgeVal1 = edgeYBase1 + triSIMD.stepC1;
						edgeVal2 = edgeYBase2 + triSIMD.stepC2;
					}
				}
			});

			mFragmentBuf.clear();
			for (auto i = 0; i < mTiles.size(); i++)
			{
				mFragmentBuf.insert(mFragmentBuf.end(), mTiles[i].fragmentBuf.begin(), mTiles[i].fragmentBuf.end());
			}
		}

		void Renderer::Rasterization()
		{
			//mFragmentBuf.clear();
			//for (auto i = 0; i < mRasterTriangleBuf.size(); i++)
			//{
			//	RasterTriangle& tri = mRasterTriangleBuf[i];

			//	int minX = Math::Max(0, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) >> 4);
			//	int maxX = Math::Min(mpFrameBuffer->GetWidth() - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) >> 4);
			//	int minY = Math::Max(0, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) >> 4);
			//	int maxY = Math::Min(mpFrameBuffer->GetHeight() - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) >> 4);
			//	minX -= minX % 2;
			//	minY -= minY % 2;

			//	TriangleSIMD triSIMD;
			//	triSIMD.Load(tri);

			//	Vector2i sampleCoord = Vector2i(minX << 4, minY << 4);
			//	Vec2i_SSE rasterSamplePos = Vec2i_SSE(IntSSE(sampleCoord.x + 8, sampleCoord.x + 24, sampleCoord.x + 8, sampleCoord.x + 24), IntSSE(sampleCoord.y + 8, sampleCoord.y + 8, sampleCoord.y + 24, sampleCoord.y + 24));
			//	IntSSE edgeVal0 = triSIMD.EdgeFunc0(rasterSamplePos);
			//	IntSSE edgeVal1 = triSIMD.EdgeFunc1(rasterSamplePos);
			//	IntSSE edgeVal2 = triSIMD.EdgeFunc2(rasterSamplePos);

			//	Vector2i pixelCrd;
			//	for (pixelCrd.y = minY; pixelCrd.y <= maxY; pixelCrd.y += 2)
			//	{
			//		IntSSE edgeYBase0 = edgeVal0;
			//		IntSSE edgeYBase1 = edgeVal1;
			//		IntSSE edgeYBase2 = edgeVal2;

			//		for (pixelCrd.x = minX; pixelCrd.x <= maxX; pixelCrd.x += 2)
			//		{
			//			BoolSSE inside = (edgeVal0 | edgeVal1 | edgeVal2) >= IntSSE(Math::EDX_ZERO);
			//			if (SSE::Any(inside))
			//			{
			//				sampleCoord = Vector2i(pixelCrd.x << 4, pixelCrd.y << 4);
			//				rasterSamplePos = Vec2i_SSE(IntSSE(sampleCoord.x + 8, sampleCoord.x + 24, sampleCoord.x + 8, sampleCoord.x + 24), IntSSE(sampleCoord.y + 8, sampleCoord.y + 8, sampleCoord.y + 24, sampleCoord.y + 24));
			//				triSIMD.CalcBarycentricCoord(rasterSamplePos.x, rasterSamplePos.y);

			//				const ProjectedVertex& v0 = mProjectedVertexBuf[triSIMD.vId0];
			//				const ProjectedVertex& v1 = mProjectedVertexBuf[triSIMD.vId1];
			//				const ProjectedVertex& v2 = mProjectedVertexBuf[triSIMD.vId2];

			//				QuadFragment frag;
			//				BoolSSE zTest = mpFrameBuffer->ZTestQuad(frag.GetDepth(v0, v1, v2, triSIMD.lambda0, triSIMD.lambda1), pixelCrd.x, pixelCrd.y, inside);
			//				if (SSE::Any(zTest))
			//				{
			//					frag.vId0 = triSIMD.vId0;
			//					frag.vId1 = triSIMD.vId1;
			//					frag.vId2 = triSIMD.vId2;
			//					frag.textureId = triSIMD.textureId;
			//					frag.lambda0 = triSIMD.lambda0;
			//					frag.lambda1 = triSIMD.lambda1;
			//					frag.pixelCoord = pixelCrd;
			//					frag.coverageMask = inside;

			//					mFragmentBuf.push_back(frag);
			//				}
			//			}

			//			edgeVal0 += triSIMD.stepB0;
			//			edgeVal1 += triSIMD.stepB1;
			//			edgeVal2 += triSIMD.stepB2;
			//		}

			//		edgeVal0 = edgeYBase0 + triSIMD.stepC0;
			//		edgeVal1 = edgeYBase1 + triSIMD.stepC1;
			//		edgeVal2 = edgeYBase2 + triSIMD.stepC2;
			//	}
			//}
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
				mpPixelShader->Shade(frag,
					Matrix::TransformPoint(Vector3::ZERO, mGlobalRenderStates.GetModelViewInvMatrix()),
					Vector3(-1, 1, -1),
					position,
					normal,
					texCoord,
					mGlobalRenderStates);

			});

			for (auto& frag : mFragmentBuf)
			{
				for (auto sId = 0; sId < mpFrameBuffer->GetSampleCount(); sId++)
				{
					int maskShift = sId << 2;
					if (frag.coverageMask.GetBit(maskShift) != 0)
					{
						mpFrameBuffer->SetPixel(Color(frag.shadingResult.x[0], frag.shadingResult.y[0], frag.shadingResult.z[0]), frag.x, frag.y, sId);
					}
					if (frag.coverageMask.GetBit(maskShift + 1) != 0)
					{
						mpFrameBuffer->SetPixel(Color(frag.shadingResult.x[1], frag.shadingResult.y[1], frag.shadingResult.z[1]), frag.x + 1, frag.y, sId);
					}
					if (frag.coverageMask.GetBit(maskShift + 2) != 0)
					{
						mpFrameBuffer->SetPixel(Color(frag.shadingResult.x[2], frag.shadingResult.y[2], frag.shadingResult.z[2]), frag.x, frag.y + 1, sId);
					}
					if (frag.coverageMask.GetBit(maskShift + 3) != 0)
					{
						mpFrameBuffer->SetPixel(Color(frag.shadingResult.x[3], frag.shadingResult.y[3], frag.shadingResult.z[3]), frag.x + 1, frag.y + 1, sId);
					}
				}
			}

			mpFrameBuffer->Resolve();
		}

		const float* Renderer::GetBackBuffer() const
		{
			return mpFrameBuffer->GetColorBuffer();
		}
	}
}