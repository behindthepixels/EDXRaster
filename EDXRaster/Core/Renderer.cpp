
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

			struct RasterTriangle
			{
				Vector2i v0, v1, v2;
				int B0, C0, B1, C1, B2, C2;

				float invDet;
				int triId;

				float lambda0, lambda1; // Barycentric coordinates

				bool Setup(const Vector3& a, const Vector3& b, const Vector3& c, const int id)
				{
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
					triId = id;

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

			for (auto i = 0; i < mIndexBuf.GetTriangleCount(); i++)
			{
				const uint* pIndex = mIndexBuf.GetIndex(i);
				const ProjectedVertex& v0 = mProjectedVertexBuf[pIndex[0]];
				const ProjectedVertex& v1 = mProjectedVertexBuf[pIndex[1]];
				const ProjectedVertex& v2 = mProjectedVertexBuf[pIndex[2]];

				RasterTriangle tri;
				if (!tri.Setup(v0.projectedPos.xyz(),
					v1.projectedPos.xyz(),
					v2.projectedPos.xyz(),
					i))
					continue;

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
							if (mpFrameBuffer->ZTest(frag.GetDepth(v0, v1, v2, tri.lambda0, tri.lambda1), vP.x, vP.y))
							{
								frag.SetupAndInterpolate(v0, v1, v2, tri.lambda0, tri.lambda1);
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

		void Renderer::Clipping(IndexBuffer* pSrcIdxBuf)
		{
			mIndexBuf.ResizeBuffer(0);
			Clipper::Clip(mProjectedVertexBuf, &mIndexBuf, pSrcIdxBuf);

			for (auto& vertex : mProjectedVertexBuf)
			{
				float invW = 1.0f / vertex.projectedPos.w;
				vertex.projectedPos.x *= invW;
				vertex.projectedPos.y *= invW;
				vertex.projectedPos.z *= invW;
				vertex.projectedPos.w *= invW;

				vertex.projectedPos = Matrix::TransformPoint(vertex.projectedPos, mGlobalRenderStates.mRasterMatrix);
				vertex.invW = invW;
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