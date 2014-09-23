#pragma once

#include "RenderState.h"
#include "Shader.h"
#include "RasterTriangle.h"
#include "Tile.h"
#include "../Utils/InputBuffer.h"
#include "Windows/Thread.h"
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
			RefPtr<class Rasterizer> mpRasterizer;
			RefPtr<class VertexShader> mpVertexShader;
			RefPtr<class PixelShader> mpPixelShader;
			RefPtr<class Scene> mpScene;

			vector<ProjectedVertex> mProjectedVertexBuf;
			vector<RasterTriangle> mRasterTriangleBuf;
			vector<Fragment> mFragmentBuf;
			vector<vector<IntSSE>> mTiledShadingResultBuf;

			vector<Tile> mTiles;
			Vector2i mTileDim;

			int mNumCores;
			int mFrameCount;
			bool mHierarchicalRasterize;

		public:
			void Initialize(uint iScreenWidth, uint iScreenHeight);
			void Resize(uint iScreenWidth, uint iScreenHeight);
			void SetTransform(const class Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster);
			void RenderMesh(const class Mesh& mesh);

			const _byte* GetBackBuffer() const;
			void SetMSAAMode(const int msaaCountLog2);
			void SetTextureFilter(const TextureFilter filter) { mGlobalRenderStates.mTexFilter = filter; }
			void SetHierarchicalRasterize(const bool hRas) { mHierarchicalRasterize = hRas; }

		private:
			void VertexProcessing(const IVertexBuffer* pVertexBuf);
			void Clipping(IndexBuffer* pIndexBuf, const vector<uint>& texIdBuf);
			void TiledRasterization();
			void RasterizeTile(Tile& tile);
			void FragmentProcessing();
			void UpdateFrameBuffer();
		};

	}
}