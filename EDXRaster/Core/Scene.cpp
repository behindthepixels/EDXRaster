#include "Scene.h"

#include "../Utils/Mesh.h"

namespace EDX
{
	namespace RasterRenderer
	{
		void Scene::AddMesh(Mesh* pMesh)
		{
			mMeshes.push_back(pMesh);
		}
	}
}