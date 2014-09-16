#pragma once

#include "EDXPrerequisites.h"
#include "Graphics/Texture.h"
#include "Math/Matrix.h"
#include "Memory/RefPtr.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class RenderState
		{
		public:
			Matrix mModelViewMatrix;
			Matrix mModelViewInvMatrix;
			Matrix mProjMatrix;
			Matrix mModelViewProjMatrix;
			Matrix mRasterMatrix;

			uint mSampleCountLog2;
			bool mBackFaceCull;
			TextureFilter mTexFilter;

			vector<RefPtr<Texture2D<Color>>> mTextureSlots;

		public:
			const Matrix& GetModelViewProjMatrix() const { return mModelViewProjMatrix; }
			const Matrix& GetModelViewMatrix() const { return mModelViewMatrix; }
			const Matrix& GetModelViewInvMatrix() const { return mModelViewInvMatrix; }
			const Matrix& GetProjectMatrix() const { return mProjMatrix; }
			const Matrix& GetRasterMatrix() const { return mRasterMatrix; }

			const TextureFilter GetTextureFilter() const { return mTexFilter; }
		};
	}
}