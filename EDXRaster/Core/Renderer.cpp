
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
			mpFrameBuffer->Init(iScreenWidth, iScreenHeight);

			if (!mpScene)
			{
				mpScene = new Scene;
			}

			mpVertexShader = new DefaultVertexShader;
			mpPixelShader = new QuadLambertianPixelShader;
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
			mpFrameBuffer->Clear();

			VertexProcessing(mesh.GetVertexBuffer());
			Clipping(mesh.GetIndexBuffer());

			mFragmentBuf.clear();
			for (auto i = 0; i < mRasterTriangleBuf.size(); i++)
			//parallel_for(0, (int)mRasterTriangleBuf.size(), [&](int i)
			{
				RasterTriangle& tri = mRasterTriangleBuf[i];

				int minX = Math::Max(0, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) / 16);
				int maxX = Math::Min(mpFrameBuffer->GetWidth() - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) / 16);
				int minY = Math::Max(0, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) / 16);
				int maxY = Math::Min(mpFrameBuffer->GetHeight() - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) / 16);
				minX -= minX % 2;
				minY -= minY % 2;

				TriangleSIMD triSIMD;
				triSIMD.Load(tri);

				Vector2i sampleCoord = 16 * Vector2i(minX, minY);
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
						BoolSSE inside = (edgeVal0 | edgeVal1 | edgeVal2) >= IntSSE(Math::EDX_ZERO);
						if (SSE::Any(inside))
						{
							sampleCoord = 16 * Vector2i(pixelCrd.x, pixelCrd.y);
							rasterSamplePos = Vec2i_SSE(IntSSE(sampleCoord.x + 8, sampleCoord.x + 24, sampleCoord.x + 8, sampleCoord.x + 24), IntSSE(sampleCoord.y + 8, sampleCoord.y + 8, sampleCoord.y + 24, sampleCoord.y + 24));
							triSIMD.CalcBarycentricCoord(rasterSamplePos.x, rasterSamplePos.y);

							const ProjectedVertex& v0 = mProjectedVertexBuf[triSIMD.vId0];
							const ProjectedVertex& v1 = mProjectedVertexBuf[triSIMD.vId1];
							const ProjectedVertex& v2 = mProjectedVertexBuf[triSIMD.vId2];

							QuadFragment frag;
							BoolSSE zTest = mpFrameBuffer->ZTestQuad(frag.GetDepth(v0, v1, v2, triSIMD.lambda0, triSIMD.lambda1), pixelCrd.x, pixelCrd.y, inside);
							if (SSE::Any(zTest))
							{
								frag.vId0 = triSIMD.vId0;
								frag.vId1 = triSIMD.vId1;
								frag.vId2 = triSIMD.vId2;
								frag.lambda0 = triSIMD.lambda0;
								frag.lambda1 = triSIMD.lambda1;
								frag.pixelCoord = pixelCrd;
								frag.insideMask = inside;

								mFragmentBuf.push_back(frag);
							}
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

		void Renderer::Clipping(IndexBuffer* pIndexBuf)
		{
			mRasterTriangleBuf.clear();
			Clipper::Clip(mProjectedVertexBuf, pIndexBuf, mGlobalRenderStates.GetRasterMatrix(), mRasterTriangleBuf);

			parallel_for(0, (int)mProjectedVertexBuf.size(), [&](int i)
			{
				ProjectedVertex& vertex = mProjectedVertexBuf[i];
				vertex.invW = 1.0f / vertex.projectedPos.w;
				vertex.projectedPos.z *= vertex.invW;
			});
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

				frag.Interpolate(v0, v1, v2, frag.lambda0, frag.lambda1);
				Vec3f_SSE c = mpPixelShader->Shade(frag,
					Matrix::TransformPoint(Vector3::ZERO, mGlobalRenderStates.GetModelViewInvMatrix()),
					Vector3(-1, 1, -1));

				if (frag.insideMask[0] != 0 && mpFrameBuffer->ZTest(frag.depth[0], frag.pixelCoord.x, frag.pixelCoord.y))
				{
					mpFrameBuffer->SetColor(Color(c.x[0], c.y[0], c.z[0]), frag.pixelCoord.x, frag.pixelCoord.y);
				}
				if (frag.insideMask[1] != 0 && mpFrameBuffer->ZTest(frag.depth[1], frag.pixelCoord.x + 1, frag.pixelCoord.y))
				{
					mpFrameBuffer->SetColor(Color(c.x[1], c.y[1], c.z[1]), frag.pixelCoord.x + 1, frag.pixelCoord.y);
				}
				if (frag.insideMask[2] != 0 && mpFrameBuffer->ZTest(frag.depth[2], frag.pixelCoord.x, frag.pixelCoord.y + 1))
				{
					mpFrameBuffer->SetColor(Color(c.x[2], c.y[2], c.z[2]), frag.pixelCoord.x, frag.pixelCoord.y + 1);
				}
				if (frag.insideMask[3] != 0 && mpFrameBuffer->ZTest(frag.depth[3], frag.pixelCoord.x + 1, frag.pixelCoord.y + 1))
				{
					mpFrameBuffer->SetColor(Color(c.x[3], c.y[3], c.z[3]), frag.pixelCoord.x + 1, frag.pixelCoord.y + 1);
				}
			});
		}

		const float* Renderer::GetBackBuffer() const
		{
			return mpFrameBuffer->GetColorBuffer();
		}
	}
}