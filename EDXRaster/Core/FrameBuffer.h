#pragma once

#include "Memory/Array.h"
#include "Graphics/Color.h"
#include "SIMD/SSE.h"

namespace EDX
{
	namespace RasterRenderer
	{
		typedef Array<2, Color> Array2C;
		typedef Array<3, Color> Array3C;

		class FrameBuffer
		{
		private:
			Array3C mColorBufferMS;
			Array2C mColorBuffer;
			Array<3, FloatSSE> mDepthBuffer;
			vector<Array<3, FloatSSE>> mTiledDepthBuffer;
			uint mTileDimX, mTileDimY;
			uint mResX, mResY;

			uint mSampleCount;
			uint mMultiSampleLevel;

		public:
			static const int MultiSampleOffsets[][64];

		public:
			void Init(uint iWidth, uint iHeight, const Vector2i& tileDim, uint sampleCountLog2 = 0);
			void SetPixel(const Color& c, const int x, const int y, const uint sId);
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
			const float* GetColorBuffer() const
			{
				return mSampleCount == 1 ? (float*)mColorBufferMS.Data() : (float*)mColorBuffer.Data();
			}

			void Clear(const bool clearColor = true, const bool clearDepth = true);
		};
	}
}