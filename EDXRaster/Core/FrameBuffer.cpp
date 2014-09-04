#include "FrameBuffer.h"
#include "Math/EDXMath.h"

namespace EDX
{
	namespace RasterRenderer
	{
		void FrameBuffer::Init(uint iWidth, uint iHeight, uint sampleCountLog2)
		{
			mMultiSampleLevel = sampleCountLog2;
			mSampleCount = 1 << sampleCountLog2;

			mResX = iWidth;
			mResY = iHeight;
			mColorBufferMS.Init(Vector3i(mSampleCount, iWidth, iHeight));
			mColorBuffer.Init(Vector2i(iWidth, iHeight));
			mDepthBuffer.Init(Vector3i(mSampleCount, iWidth, iHeight));
		}

		void FrameBuffer::SetPixel(const Color& c, const int x, const int y, const uint sId)
		{
			mColorBufferMS[Vector3i(sId, x, mResY - 1 - y)] = c;
		}

		bool FrameBuffer::ZTest(const float d, const int x, const int y, const uint sId)
		{
			float& currDepth = mDepthBuffer[Vector3i(sId, x, mResY - 1 - y)];
			if (d > currDepth)
				return false;

			currDepth = d;
			return true;
		}

		BoolSSE FrameBuffer::ZTestQuad(const FloatSSE& d, const int x, const int y, const uint sId, const BoolSSE& mask)
		{
			float& currDepth00 = mDepthBuffer[Vector3i(sId, x, mResY - 1 - y)];
			float& currDepth01 = mDepthBuffer[Vector3i(sId, x + 1, mResY - 1 - y)];
			float& currDepth10 = mDepthBuffer[Vector3i(sId, x, mResY - 1 - y - 1)];
			float& currDepth11 = mDepthBuffer[Vector3i(sId, x + 1, mResY - 1 - y - 1)];

			bool lt00 = d[0] <= currDepth00;
			bool lt01 = d[1] <= currDepth01;
			bool lt10 = d[2] <= currDepth10;
			bool lt11 = d[3] <= currDepth11;

			if (lt00 && mask[0] != 0)
				currDepth00 = d[0];
			if (lt01 && mask[1] != 0)
				currDepth01 = d[1];
			if (lt10 && mask[2] != 0)
				currDepth10 = d[2];
			if (lt11 && mask[3] != 0)
				currDepth11 = d[3];

			return BoolSSE(lt00, lt01, lt10, lt11);
		}

		void FrameBuffer::Resolve()
		{
			const float invSampleCount = 1.0f / float(mSampleCount);
			for (auto y = 0; y < mResY; y++)
			{
				for (auto x = 0; x < mResX; x++)
				{
					Color c = 0;
					for (auto s = 0; s < mSampleCount; s++)
					{
						c += mColorBufferMS[Vector3i(s, x, y)];
					}
					c *= invSampleCount;
					mColorBuffer[Vector2i(x, y)] = c;
				}
			}
		}

		void FrameBuffer::Clear(const bool clearColor, const bool clearDepth)
		{
			if (clearColor)
			{
				mColorBuffer.Clear();
				mColorBufferMS.Clear();
			}

			for (auto i = 0; i < mDepthBuffer.LinearSize(); i++)
				mDepthBuffer[i] = 1.0f;
		}

		const int FrameBuffer::MultiSampleOffsets[][64] =
		{
			{
				0, 0
			},
			// 2x samples
			{
				4, 4,
				-4, -4
			},

			// 4x samples
			{
				-2, -6,
				6, -2,
				-6, 2,
				2, 6
			},
			// 8x samples
			{
				1, -3,
				-1, 3,
				5, 1,
				-3, -5,
				-5, 5,
				-7, -1,
				3, 7,
				7, -7
			},
			// 16x samples
			{
				1, 1,
				-1, -3,
				-3, 2,
				4, -1,
				-5, -2,
				2, 5,
				5, 3,
				3, -5,
				-2, 6,
				0, -7,
				-4, -6,
				-6, 4,
				-8, 0,
				7, -4,
				6, 7,
				-7, -8
			},
			// 32x samples
			{
				1, 1,
				-1, -3,
				-3, 2,
				4, -1,
				-5, -2,
				2, 5,
				5, 3,
				3, -5,
				-2, 6,
				0, -7,
				-4, -6,
				-6, 4,
				-8, 0,
				7, -4,
				6, 7,
				-7, -8,

				1, 3,
				-3, -3,
				-3, 0,
				6, -2,
				-7, -1,
				3, 4,
				7, 3,
				3, -6,
				-2, 7,
				0, -4,
				-2, -5,
				-7, 6,
				-8, 3,
				4, -1,
				2, 7,
				4, -8
			}
		};
	}
}