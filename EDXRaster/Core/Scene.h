#pragma once

#include "EDXPrerequisites.h"
#include "Core/SmartPointer.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Scene
		{
		private:
			Array<UniquePtr<class Mesh>> mMeshes;

		public:
			void AddMesh(Mesh* pMesh);
		};
	}
}
