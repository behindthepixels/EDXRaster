#pragma once

#include "EDXPrerequisites.h"
#include "Math/Matrix.h"
#include "Memory/RefPtr.h"

namespace EDX
{
	template<typename T>
	class Texture;
	class Color;

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

			bool mBackFaceCull;

			vector<RefPtr<Texture<Color>>> mTextureSlots;

		public:
			const Matrix& GetModelViewProjMatrix() const { return mModelViewProjMatrix; }
			const Matrix& GetModelViewMatrix() const { return mModelViewMatrix; }
			const Matrix& GetModelViewInvMatrix() const { return mModelViewInvMatrix; }
			const Matrix& GetProjectMatrix() const { return mProjMatrix; }
			const Matrix& GetRasterMatrix() const { return mRasterMatrix; }
		};
	}
}