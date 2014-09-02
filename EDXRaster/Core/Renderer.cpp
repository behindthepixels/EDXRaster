
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
			mpPixelShader = new BlinnPhongPixelShader;
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

			for (auto i = 0; i < mRasterTriangleBuf.size(); i++)
			//parallel_for(0, (int)mRasterTriangleBuf.size(), [&](int i)
			{
				RasterTriangle& tri = mRasterTriangleBuf[i];

				int minX = Math::Max(0, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) / 16);
				int maxX = Math::Min(mpFrameBuffer->GetWidth() - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) / 16);
				int minY = Math::Max(0, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) / 16);
				int maxY = Math::Min(mpFrameBuffer->GetHeight() - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) / 16);

				TriangleSIMD triSIMD;
				triSIMD.Load(tri);

				Vector2i coord = 16 * Vector2i(minX, minY);
				Vec2i_SSE rasterPos = Vec2i_SSE(IntSSE(coord.x + 8, coord.x + 24, coord.x + 8, coord.x + 24), IntSSE(coord.y + 8, coord.y + 8, coord.y + 24, coord.y + 24));
				IntSSE edgeVal0 = triSIMD.EdgeFunc0(rasterPos);
				IntSSE edgeVal1 = triSIMD.EdgeFunc1(rasterPos);
				IntSSE edgeVal2 = triSIMD.EdgeFunc2(rasterPos);

				Vector2i vP;
				for (vP.y = minY; vP.y <= maxY; vP.y += 2)
				{
					IntSSE edgeYBase0 = edgeVal0;
					IntSSE edgeYBase1 = edgeVal1;
					IntSSE edgeYBase2 = edgeVal2;

					vP.y = Math::Min(vP.y, maxY);

					for (vP.x = minX; vP.x <= maxX; vP.x += 2)
					{
						vP.x = Math::Min(vP.x, maxX);

						BoolSSE inside = (edgeVal0 | edgeVal1 | edgeVal2) >= IntSSE(Math::EDX_ZERO);
						if (SSE::Any(inside))
						{
							coord = 16 * Vector2i(vP.x, vP.y);
							rasterPos = Vec2i_SSE(IntSSE(coord.x + 8, coord.x + 24, coord.x + 8, coord.x + 24), IntSSE(coord.y + 8, coord.y + 8, coord.y + 24, coord.y + 24));
							triSIMD.CalcBarycentricCoord(rasterPos.x, rasterPos.y);

							auto FragmentProcess = [&](int i)
							{
								Fragment frag;
								if (mpFrameBuffer->ZTest(frag.GetDepth(mProjectedVertexBuf[triSIMD.vId0],
									mProjectedVertexBuf[triSIMD.vId1],
									mProjectedVertexBuf[triSIMD.vId2],
									triSIMD.lambda0[i], triSIMD.lambda1[i]), vP.x + i % 2, vP.y + i / 2))
								{
									frag.SetupAndInterpolate(mProjectedVertexBuf[triSIMD.vId0],
										mProjectedVertexBuf[triSIMD.vId1],
										mProjectedVertexBuf[triSIMD.vId2],
										triSIMD.lambda0[i], triSIMD.lambda1[i]);
									Color c = mpPixelShader->Shade(frag,
										Matrix::TransformPoint(Vector3::ZERO, mGlobalRenderStates.GetModelViewInvMatrix()),
										Vector3(-1, 1, -1));
									mpFrameBuffer->SetColor(Color(triSIMD.lambda0[i], triSIMD.lambda1[i], 1.0f - triSIMD.lambda0[i] - triSIMD.lambda1[i]), vP.x + i % 2, vP.y + i / 2);
								}
							};

							if (inside[0] != 0)
							{
								FragmentProcess(0);
							}
							if (inside[1] != 0)
							{
								FragmentProcess(1);
							}
							if (inside[2] != 0)
							{
								FragmentProcess(2);
							}
							if (inside[3] != 0)
							{
								FragmentProcess(3);
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

		}

		const float* Renderer::GetBackBuffer() const
		{
			return mpFrameBuffer->GetColorBuffer();
		}
	}
}