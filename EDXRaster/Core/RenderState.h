#pragma once

#include "Math/Matrix.h"

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

		public:
			const Matrix& GetModelViewProjMatrix() const { return mModelViewProjMatrix; }
			const Matrix& GetModelViewMatrix() const { return mModelViewMatrix; }
			const Matrix& GetModelViewInvMatrix() const { return mModelViewInvMatrix; }
			const Matrix& GetProjectMatrix() const { return mProjMatrix; }
			const Matrix& GetRasterMatrix() const { return mRasterMatrix; }
		};
	}
}