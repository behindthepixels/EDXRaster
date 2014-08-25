#include "Mesh.h"
#include "InputBuffer.h"
#include "Graphics/ObjMesh.h"
#include "Memory/Memory.h"


namespace EDX
{
	namespace RasterRenderer
	{
		void Mesh::LoadPlane(const Vector3& pos,
			const Vector3& scl,
			const Vector3& rot,
			const float length)
		{
			ObjMesh mesh;
			mesh.LoadPlane(pos, scl, rot, length);

			mpVertexBuf = CreateVertexBuffer(&mesh.GetVertexAt(0), mesh.GetVertexCount());
			mpIndexBuf = CreateIndexBuffer(mesh.GetIndexAt(0), mesh.GetTriangleCount());
		}
	}
}