#pragma once

#include "Memory/Array.h"
#include "Graphics/Color.h"

namespace EDX
{
	namespace RasterRenderer
	{
		typedef Array<2, Color> Array2C;
		typedef Array<2, float> Array2f;

		class FrameBuffer
		{
		private:
			Array2C mColorBuffer;
			Array2f mDepthBuffer;
			uint mResX, mResY;

		public:
			static const int MultiSampleOffsets[][64];

		public:
			void Init(uint iWidth, uint iHeight);
			void SetColor(const Color& c, const int x, const int y);
			bool ZTest(const float d, const int x, const int y);
			uint GetWidth() const { return mResX; }
			uint GetHeight() const { return mResY; }
			const float* GetColorBuffer() const
			{
				return (float*)mColorBuffer.Data();
			}

			void Clear(const bool clearColor = true, const bool clearDepth = true);
		};
	}
}