#include "FrameBuffer.h"
#include "Math/EDXMath.h"

namespace EDX
{
	namespace RasterRenderer
	{
		void FrameBuffer::Init(uint iWidth, uint iHeight)
		{
			mResX = iWidth;
			mResY = iHeight;
			mPixels.Init(Vector2i(iWidth, iHeight));
		}
	}
}