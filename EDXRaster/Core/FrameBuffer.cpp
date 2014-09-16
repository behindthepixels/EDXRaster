#include "FrameBuffer.h"
#include "Tile.h"
#include "Math/EDXMath.h"

#include <ppl.h>
using namespace concurrency;

namespace EDX
{
	namespace RasterRenderer
	{
		void FrameBuffer::Init(uint iWidth, uint iHeight, const Vector2i& tileDim, uint sampleCountLog2)
		{
			mMultiSampleLevel = sampleCountLog2;
			mSampleCount = 1 << sampleCountLog2;

			mResX = iWidth;
			mResY = iHeight;
			mColorBufferMS.Init(Vector3i(mSampleCount, iWidth, iHeight));
			mColorBuffer.Init(Vector2i(iWidth, iHeight));

			mTileDimX = tileDim.x;
			mTileDimY = tileDim.y;

			mTiledDepthBuffer.resize(tileDim.x * tileDim.y);
			for (auto& it : mTiledDepthBuffer)
				it.Init(Vector3i(mSampleCount, Tile::SIZE >> 1, Tile::SIZE >> 1));
		}

		void FrameBuffer::Resize(uint iWidth, uint iHeight, const Vector2i& tileDim, uint sampleCountLog2)
		{
			mColorBuffer.Free();
			mColorBufferMS.Free();
			mTiledDepthBuffer.clear();

			Init(iWidth, iHeight, tileDim, sampleCountLog2);
		}

		void FrameBuffer::SetPixel(const Color& c, const int x, const int y, const uint sId)
		{
			mColorBufferMS[Vector3i(sId, x, mResY - 1 - y)] = c;
		}

		bool FrameBuffer::ZTest(const float d, const int x, const int y, const uint sId)
		{
			//float& currDepth = mDepthBuffer[Vector3i(sId, x, mResY - 1 - y)];
			//if (d > currDepth)
			//	return false;

			//currDepth = d;
			return true;
		}

		BoolSSE FrameBuffer::ZTestQuad(const FloatSSE& d, const int x, const int y, const uint sId, const BoolSSE& mask)
		{
			const int tileX = x >> Tile::SIZE_LOG_2;
			const int tileY = y >> Tile::SIZE_LOG_2;
			Array<3, FloatSSE>& currTileDepths = mTiledDepthBuffer[tileY * mTileDimX + tileX];

			const int intraTileX = x & (Tile::SIZE - 1);
			const int intraTileY = y & (Tile::SIZE - 1);
			FloatSSE& currDepth = currTileDepths[Vector3i(sId, intraTileX >> 1, (Tile::SIZE - 1 - intraTileY) >> 1)];

			BoolSSE ret = d <= currDepth;
			currDepth = SSE::Select(ret & mask, d, currDepth);

			return ret;
		}

		void FrameBuffer::Resolve()
		{
			if (mSampleCount == 1)
				return;

			const float invSampleCount = 1.0f / float(mSampleCount);
			parallel_for(0, (int)mColorBuffer.LinearSize(), [&](int i)
			{
				const Vector2i idx = mColorBuffer.Index(i);
				Color c = 0;
				for (auto s = 0; s < mSampleCount; s++)
				{
					c += mColorBufferMS[Vector3i(s, idx.x, idx.y)];
				}
				c *= invSampleCount;
				mColorBuffer[i] = c;
			});
		}

		void FrameBuffer::Clear(const bool clearColor, const bool clearDepth)
		{
			if (clearColor)
			{
				mColorBuffer.Clear();
				mColorBufferMS.Clear();
			}

			int tileCount = mTileDimX * mTileDimY;
			parallel_for(0, tileCount, [&](int i)
			{
				Array<3, FloatSSE>& currTileDepths = mTiledDepthBuffer[i];

				for (auto j = 0; j < currTileDepths.LinearSize(); j++)
					currTileDepths[j] = 1.0f;
			});
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