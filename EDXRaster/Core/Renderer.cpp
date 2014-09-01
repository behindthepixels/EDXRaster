
#include "Renderer.h"
#include "FrameBuffer.h"
#include "Scene.h"
#include "Clipper.h"
#include "../Utils/Mesh.h"
#include "../Utils/InputBuffer.h"
#include "Math/Matrix.h"

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
			mpPixelShader = new BlinnPhonePixelShader;
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
			{
				RasterTriangle& tri = mRasterTriangleBuf[i];

				int minX = Math::Max(0, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)) / 16);
				int maxX = Math::Min(mpFrameBuffer->GetWidth() - 1, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)) / 16);
				int minY = Math::Max(0, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)) / 16);
				int maxY = Math::Min(mpFrameBuffer->GetHeight() - 1, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)) / 16);

				Vector2i vP;
				for (vP.y = minY; vP.y <= maxY; vP.y++)
				{
					for (vP.x = minX; vP.x <= maxX; vP.x++)
					{
						Vector2i rasterPos = vP * 16 + 8 * Vector2i::UNIT_SCALE;
						if (tri.Inside(rasterPos))
						{
							tri.CalcBarycentricCoord(rasterPos.x, rasterPos.y);
							Fragment frag;
							if (mpFrameBuffer->ZTest(frag.GetDepth(mProjectedVertexBuf[tri.vId0],
								mProjectedVertexBuf[tri.vId1],
								mProjectedVertexBuf[tri.vId2],
								tri.lambda0, tri.lambda1), vP.x, vP.y))
							{
								frag.SetupAndInterpolate(mProjectedVertexBuf[tri.vId0],
									mProjectedVertexBuf[tri.vId1],
									mProjectedVertexBuf[tri.vId2],
									tri.lambda0, tri.lambda1);
								Color c = mpPixelShader->Shade(frag,
									Matrix::TransformPoint(Vector3::ZERO, mGlobalRenderStates.GetModelViewInvMatrix()),
									Vector3(-1, 1, -1));
								mpFrameBuffer->SetColor(c, vP.x, vP.y);
							}
						}
					}
				}

			}
		}

		void Renderer::VertexProcessing(const IVertexBuffer* pVertexBuf)
		{
			mProjectedVertexBuf.resize(pVertexBuf->GetVertexCount());
			for (auto i = 0; i < pVertexBuf->GetVertexCount(); i++)
			{
				mpVertexShader->Execute(mGlobalRenderStates, pVertexBuf->GetPosition(i), pVertexBuf->GetNormal(i), pVertexBuf->GetTexCoord(i), &mProjectedVertexBuf[i]);
			}
		}

		void Renderer::Clipping(IndexBuffer* pIndexBuf)
		{
			mRasterTriangleBuf.clear();
			Clipper::Clip(mProjectedVertexBuf, pIndexBuf, mGlobalRenderStates.GetRasterMatrix(), mRasterTriangleBuf);

			for (auto& vertex : mProjectedVertexBuf)
			{
				vertex.invW = 1.0f / vertex.projectedPos.w;
				vertex.projectedPos.z *= vertex.invW;
			}
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