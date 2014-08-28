
#include "Renderer.h"
#include "FrameBuffer.h"
#include "Scene.h"
#include "../Utils/Mesh.h"
#include "../Utils/InputBuffer.h"
#include "Math/Matrix.h"

#include <Windows.h>
#include <gl/gl.h>
#include <gl/glu.h>

namespace EDX
{
	namespace RasterRenderer
	{
		void Renderer::Initialize(uint iScreenWidth, uint iScreenHeight)
		{
			mpFrameBuffer = new FrameBuffer;
			mpFrameBuffer->Init(iScreenWidth, iScreenHeight);

			mpScene = new Scene;

			mpVertexShader = new DefaultVertexShader;
		}

		void Renderer::SetRenderState(const Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster)
		{
			mGlobalRenderStates.mmModelView = mModelView;
			mGlobalRenderStates.mmProj = mProj;
			mGlobalRenderStates.mmModelViewProj = mProj * mModelView;
			mGlobalRenderStates.mmRaster = mToRaster;
		}

		void Renderer::RenderMesh(const Mesh& mesh)
		{
			auto pVertexBuf = mesh.GetVertexBuffer();
			auto pIndexBuf = mesh.GetIndexBuffer();

			mProjectedVertexBuf.resize(pVertexBuf->GetVertexCount());
			for (auto i = 0; i < pVertexBuf->GetVertexCount(); i++)
			{
				mpVertexShader->Execute(mGlobalRenderStates, pVertexBuf->GetPosition(i), pVertexBuf->GetNormal(i), pVertexBuf->GetTexCoord(i), &mProjectedVertexBuf[i]);
			}

			for (auto& vertex : mProjectedVertexBuf)
			{
				float fInvW = 1.0f / vertex.projectedPos.w;
				vertex.projectedPos.x *= fInvW;
				vertex.projectedPos.y *= fInvW;
				vertex.projectedPos.z *= fInvW;
				vertex.projectedPos.w *= fInvW;

				vertex.projectedPos = Matrix::TransformPoint(vertex.projectedPos, mGlobalRenderStates.mmRaster);
			}

			glPointSize(1.0f);
			glBegin(GL_POINTS);

			struct RasterTriangle
			{
				Vector2i v0, v1, v2;
				int B0, C0, B1, C1, B2, C2;

				RasterTriangle(const Vector3& a, const Vector3& b, const Vector3& c)
				{
					// Convert to fixed point
					v0.x = (int)a.x * 16;
					v0.y = (int)a.y * 16;
					v1.x = (int)b.x * 16;
					v1.y = (int)b.y * 16;
					v2.x = (int)c.x * 16;
					v2.y = (int)c.y * 16;

					B0 = v1.y - v0.y;
					C0 = v0.x - v1.x;
					B1 = v2.y - v1.y;
					C1 = v1.x - v2.x;
					B2 = v0.y - v2.y;
					C2 = v2.x - v0.x;
				}

				bool Inside(const Vector2i& p)
				{
					return B0 * (p.x - v0.x) + C0 * (p.y - v0.y) <= 0 &&
						B1 * (p.x - v1.x) + C1 * (p.y - v1.y) <= 0 &&
						B2 * (p.x - v2.x) + C2 * (p.y - v2.y) <= 0;
				}
			};

			for (auto i = 0; i < pIndexBuf->GetTriangleCount(); i++)
			{
				const uint* pIndex = pIndexBuf->GetIndex(i);
				const Vector3& vA = mProjectedVertexBuf[pIndex[0]].projectedPos.xyz();
				const Vector3& vB = mProjectedVertexBuf[pIndex[1]].projectedPos.xyz();
				const Vector3& vC = mProjectedVertexBuf[pIndex[2]].projectedPos.xyz();

				RasterTriangle tri = RasterTriangle(vA, vB, vC);

				int minX = Math::Max(0, Math::Min(tri.v0.x, Math::Min(tri.v1.x, tri.v2.x)));
				int maxX = Math::Min(mpFrameBuffer->GetWidth() * 16, Math::Max(tri.v0.x, Math::Max(tri.v1.x, tri.v2.x)));
				int minY = Math::Max(0, Math::Min(tri.v0.y, Math::Min(tri.v1.y, tri.v2.y)));
				int maxY = Math::Min(mpFrameBuffer->GetHeight() * 16, Math::Max(tri.v0.y, Math::Max(tri.v1.y, tri.v2.y)));

				Vector2i vP;
				for (vP.y = minY; vP.y <= maxY; vP.y += 16)
				{
					for (vP.x = minX; vP.x <= maxX; vP.x += 16)
					{
						if (tri.Inside(vP + 8 * Vector2i::UNIT_SCALE))
							glVertex2f(vP.x / 16, vP.y / 16);
					}
				}

			}
			glEnd();

			// 		glBegin(GL_TRIANGLES);
			// 		for(auto i = 0; i < mesh.GetTriangleCount(); i++)
			// 		{
			// 			const uint* pIndex = mesh.GetIndexAt(i);
			// 			glVertex2f(vVertOut[pIndex[0]].vTransformedPos.x, vVertOut[pIndex[0]].vTransformedPos.y);
			// 			glVertex2f(vVertOut[pIndex[1]].vTransformedPos.x, vVertOut[pIndex[1]].vTransformedPos.y);
			// 			glVertex2f(vVertOut[pIndex[2]].vTransformedPos.x, vVertOut[pIndex[2]].vTransformedPos.y);
			// 		}
			// 		glEnd();

		}
	}
}