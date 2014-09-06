#pragma once

#include "RenderState.h"
#include "Shader.h"
#include "RasterTriangle.h"
#include "../Utils/InputBuffer.h"
#include "Windows/Thread.h"
#include "Memory/RefPtr.h"
#include "EDXPrerequisites.h"

namespace EDX
{
	namespace RasterRenderer
	{
		struct Tile
		{
			static const int SIZE_LOG_2 = 5;
			static const int SIZE = 1 << SIZE_LOG_2;

			Vector2i minCoord, maxCoord;
			vector<uint> triangleIds;
			vector<QuadFragment> fragmentBuf;

			Tile(const Vector2i& min, const Vector2i& max)
				: minCoord(min), maxCoord(max)
			{
			}
		};

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

			int FrameCount;

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
			void RasterizeTile_Hierarchical(Tile& tile);
			void FragmentProcessing();
		};

	}
}