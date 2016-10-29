#pragma once

#include "EDXPrerequisites.h"
#include "Graphics/Texture.h"
#include "Math/Matrix.h"
#include "Core/SmartPointer.h"

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

			uint MultiSampleLevel;
			bool BackFaceCull;
			TextureFilter TexFilter;

			int FrameCount;
			bool HierarchicalRasterize;

			const Array<UniquePtr<Texture2D<Color>>>* TextureSlots = nullptr;

		private:
			RenderStates()
				: FrameCount(0)
			{
			}
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
			void DefaultSettings()
			{
				MultiSampleLevel = 0;
				BackFaceCull = true;
				HierarchicalRasterize = true;
				TexFilter = TextureFilter::TriLinear;
			}

			const Matrix& GetModelViewProjMatrix() const { return ModelViewProjMatrix; }
			const Matrix& GetModelViewMatrix() const { return ModelViewMatrix; }
			const Matrix& GetModelViewInvMatrix() const { return ModelViewInvMatrix; }
			const Matrix& GetProjectMatrix() const { return ProjMatrix; }
			const Matrix& GetRasterMatrix() const { return RasterMatrix; }
			const TextureFilter GetTextureFilter() const { return TexFilter; }
		};
	}
}