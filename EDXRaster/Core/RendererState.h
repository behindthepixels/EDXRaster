#pragma once

#include "Math/Matrix.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class RendererState
		{
		public:
			Matrix mmModelView;
			Matrix mmProj;
			Matrix mmModelViewProj;
			Matrix mmRaster;

		public:
			const Matrix& GetModelViewProjMatrix() const { return mmModelViewProj; }
			const Matrix& GetRasterMatrix() const { return mmRaster; }
		};
	}
}