#pragma once

#include "RendererState.h"
#include "FrameBuffer.h"
#include "Shader.h"
#include "../Utils/Mesh.h"

#include "Math/Matrix.h"
#include "Memory/RefPtr.h"

#include "EDXPrerequisites.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Renderer
		{
		private:
			RendererState mGlobalRenderStates;
			FrameBuffer mFrameBuffer;
			RefPtr<VertexShader> mpVertexShader;

		public:
			void Initialize(uint iScreenWidth, uint iScreenHeight);
			void SetRenderState(const Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster);
			void RenderMesh(const Mesh& mesh);
		};

	}
}