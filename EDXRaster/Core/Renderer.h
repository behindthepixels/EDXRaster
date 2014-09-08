#pragma once

#include "RenderState.h"
#include "Shader.h"
#include "RasterTriangle.h"
#include "Rasterizer.h"
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
			RefPtr<class VertexShader> mpVertexShader;
			RefPtr<class QuadPixelShader> mpPixelShader;
			RefPtr<class Scene> mpScene;

			vector<ProjectedVertex> mProjectedVertexBuf;
			vector<RasterTriangle> mRasterTriangleBuf;
			vector<QuadFragment> mFragmentBuf;
			vector<vector<Vec3f_SSE>> mTiledShadingResultBuf;

			vector<Tile> mTiles;
			Vector2i mTileDim;

			int mNumCores;
			int mFrameCount;

		public:
			void Initialize(uint iScreenWidth, uint iScreenHeight);
			void SetRenderState(const class Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster);
			void RenderMesh(const class Mesh& mesh);

			const float* GetBackBuffer() const;

		private:
			void VertexProcessing(const IVertexBuffer* pVertexBuf);
			void Clipping(IndexBuffer* pIndexBuf, const vector<uint>& texIdBuf);
			void TiledRasterization();
			void RasterizeTile(Tile& tile, uint tileIdx);
			void TrivialAcceptTriangle(Tile& tile, uint tileIdx, const Vector2i& blockMin, const Vector2i & blockMax, const RasterTriangle& tri);
			void RasterizeTile_Hierarchical(Tile& tile, uint tileIdx, const uint blockSize);
			void CoarseRasterize(Tile& tile,
				const uint tileIdx,
				const Tile::TriangleRef& triRef,
				const uint blockSize,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri);
			void FineRasterize(Tile& tile,
				const uint tileIdx,
				const Tile::TriangleRef& triRef,
				const Vector2i& blockMin,
				const Vector2i& blockMax,
				const RasterTriangle& tri);
			void FragmentProcessing();
		};

	}
}