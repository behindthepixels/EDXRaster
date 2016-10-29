#include "Scene.h"

#include "../Utils/Mesh.h"
#include "../Utils/InputBuffer.h"

namespace EDX
{
	namespace RasterRenderer
	{
		void Scene::AddMesh(Mesh* pMesh)
		{
			mMeshes.Add(UniquePtr<Mesh>(pMesh));
		}
	}
}