#pragma once

#include "EDXPrerequisites.h"
#include "Memory/RefPtr.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Scene
		{
		private:
			vector<RefPtr<class Mesh>> mMeshes;

		public:
			void AddMesh(Mesh* pMesh);
		};
	}
}
