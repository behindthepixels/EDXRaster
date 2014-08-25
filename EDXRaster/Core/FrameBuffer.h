#pragma once

#include "Memory/Array.h"
#include "Graphics/Color.h"

namespace EDX
{
	namespace RasterRenderer
	{
		typedef Array<2, Color> Array2C;

		class FrameBuffer
		{
		private:
			Array2C mPixels;
			uint mResX, mResY;

		public:
			void Init(uint iWidth, uint iHeight);
			uint GetWidth() const { return mResX; }
			uint GetHeight() const { return mResY; }
		};
	}
}