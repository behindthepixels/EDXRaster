#pragma once

#include "Memory/Array.h"
#include "Graphics/Color.h"
#include "SIMD/SSE.h"

namespace EDX
{
	namespace RasterRenderer
	{
		typedef Array<2, Color4b> Array2C;
		typedef Array<3, Color4b> Array3C;

		class FrameBuffer
		{
		private:
			Array3C mColorBufferMS;
			Array2C mColorBuffer;
			vector<Array<3, FloatSSE>> mTiledDepthBuffer;
			uint mTileDimX, mTileDimY;
			uint mResX, mResY;

			uint mSampleCount;
			uint mMultiSampleLevel;

		public:
			static const int MultiSampleOffsets[][64];

		public:
			void Init(uint iWidth, uint iHeight, const Vector2i& tileDim, uint sampleCountLog2 = 0);
			void Resize(uint iWidth, uint iHeight, const Vector2i& tileDim, uint sampleCountLog2 = 0);

			void SetPixel(const Color4b& c, const int x, const int y, const uint sId);
			bool ZTest(const float d, const int x, const int y, const uint sId);
			BoolSSE ZTestQuad(const FloatSSE& d, const int x, const int y, const uint sId, const BoolSSE& mask);
			void Resolve();

			uint GetSampleCount() const
			{
				return mSampleCount;
			}
			uint GetMultiSampleLevel() const
			{
				return mMultiSampleLevel;
			}
			uint GetWidth() const
			{
				return mResX;
			}
			uint GetHeight() const
			{
				return mResY;
			}
			const _byte* GetColorBuffer() const
			{
				return mSampleCount == 1 ? (_byte*)mColorBufferMS.Data() : (_byte*)mColorBuffer.Data();
			}

			void Clear(const bool clearColor = true, const bool clearDepth = true);
		};
	}
}