#pragma once

#include "RenderState.h"
#include "Shader.h"
#include "RasterTriangle.h"
#include "../Utils/InputBuffer.h"
#include "Memory/RefPtr.h"
#include "EDXPrerequisites.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Renderer
		{
		private:
			RenderState mGlobalRenderStates;
			RefPtr<class FrameBuffer> mpFrameBuffer;
			RefPtr<class VertexShader> mpVertexShader;
			RefPtr<class QuadPixelShader> mpPixelShader;
			RefPtr<class Scene> mpScene;

			vector<ProjectedVertex> mProjectedVertexBuf;
			vector<RasterTriangle> mRasterTriangleBuf;
			vector<QuadFragment> mFragmentBuf;

		public:
			void Initialize(uint iScreenWidth, uint iScreenHeight);
			void SetRenderState(const class Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster);
			void RenderMesh(const class Mesh& mesh);

			const float* GetBackBuffer() const;

		private:
			void VertexProcessing(const IVertexBuffer* pVertexBuf);
			void Clipping(IndexBuffer* pIndexBuf);
			void FragmentProcessing();
		};

	}
}