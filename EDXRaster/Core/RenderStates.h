#pragma once

#include "EDXPrerequisites.h"
#include "Graphics/Texture.h"
#include "Math/Matrix.h"
#include "Memory/RefPtr.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class RenderStates
		{
		public:
			Matrix ModelViewMatrix;
			Matrix ModelViewInvMatrix;
			Matrix ProjMatrix;
			Matrix ModelViewProjMatrix;
			Matrix RasterMatrix;

			uint SampleCountLog2;
			bool BackFaceCull;
			TextureFilter TexFilter;

			int FrameCount;
			bool HierarchicalRasterize;

			vector<RefPtr<Texture2D<Color>>> TextureSlots;

		private:
			RenderStates() {}
			static RenderStates* mpInstance;

		public:
			static RenderStates* Instance()
			{
				if (!mpInstance)
					mpInstance = new RenderStates;

				return mpInstance;
			}
			static void DeleteInstance()
			{
				if (mpInstance)
				{
					delete mpInstance;
					mpInstance = nullptr;
				}
			}

		public:
			const Matrix& GetModelViewProjMatrix() const { return ModelViewProjMatrix; }
			const Matrix& GetModelViewMatrix() const { return ModelViewMatrix; }
			const Matrix& GetModelViewInvMatrix() const { return ModelViewInvMatrix; }
			const Matrix& GetProjectMatrix() const { return ProjMatrix; }
			const Matrix& GetRasterMatrix() const { return RasterMatrix; }
			const TextureFilter GetTextureFilter() const { return TexFilter; }
		};
	}
}