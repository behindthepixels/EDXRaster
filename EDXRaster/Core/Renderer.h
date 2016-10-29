#pragma once

#include "EDXPrerequisites.h"
#include "RenderStates.h"
#include "Shader.h"
#include "RasterTriangle.h"
#include "Tile.h"
#include "../Utils/InputBuffer.h"
#include "Windows/Threading.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Renderer
		{
		private:
			UniquePtr<class FrameBuffer> mpFrameBuffer;
			UniquePtr<class Rasterizer> mpRasterizer;
			UniquePtr<class VertexShader> mpVertexShader;
			UniquePtr<class PixelShader> mpPixelShader;
			UniquePtr<class Scene> mpScene;

			Array<ProjectedVertex> mProjectedVertexBuf;
			Array<ProjectedVertex>* mpDistributedProjVertexBuf;
			Array<RasterTriangle>* mpRasterTriangleBuf;
			Array<Fragment> mFragmentBuf;
			Array<Array<IntSSE>> mTiledShadingResultBuf;

			Array<Tile> mTiles;
			Vector2i mTileDim;

			int mNumCores;
			bool mWriteFrames;

		public:
			~Renderer();

		public:
			void Initialize(uint iScreenWidth, uint iScreenHeight);
			void Resize(uint iScreenWidth, uint iScreenHeight);
			void SetTransform(const class Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster);
			void RenderMesh(const class Mesh& mesh);

			void WriteFrameToFile() const;
			const _byte* GetBackBuffer() const;
			void SetMSAAMode(const int msaaCountLog2);
			void SetTextureFilter(const TextureFilter filter) { RenderStates::Instance()->TexFilter = filter; }
			void SetHierarchicalRasterize(const bool hRas) { RenderStates::Instance()->HierarchicalRasterize = hRas; }
			void SetWriteFrames(const bool wf) { mWriteFrames = wf; }

		private:
			void VertexProcessing(const IVertexBuffer* pVertexBuf);
			void Clipping(IndexBuffer* pIndexBuf, const Array<uint>& texIdBuf);
			void TiledRasterization();
			void RasterizeTile(Tile& tile);
			void FragmentProcessing();
			void UpdateFrameBuffer();
		};

	}
}