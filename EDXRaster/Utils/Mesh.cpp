#include "Mesh.h"
#include "InputBuffer.h"
#include "Graphics/ObjMesh.h"
#include "Memory/Memory.h"


namespace EDX
{
	namespace RasterRenderer
	{
		void Mesh::LoadMesh(const Vector3& pos,
			const Vector3& scl,
			const Vector3& rot,
			const char* path)
		{
			ObjMesh mesh;
			mesh.LoadFromObj(pos, scl, rot, path);

			mpVertexBuf = CreateVertexBuffer(&mesh.GetVertexAt(0), mesh.GetVertexCount());
			mpIndexBuf = CreateIndexBuffer(mesh.GetIndexAt(0), mesh.GetTriangleCount());

			// Initialize materials
			mTexIdx = mesh.GetMaterialIdxBuf();
			const auto& materialInfo = mesh.GetMaterialInfo();
			for (auto i = 0; i < materialInfo.size(); i++)
			{
				if (materialInfo[i].strTexturePath[0])
					mTextures.push_back(new ImageTexture<Color>(materialInfo[i].strTexturePath));
				else
					mTextures.push_back(new ConstantTexture<Color>(materialInfo[i].color));
			}
		}

		void Mesh::LoadPlane(const Vector3& pos,
			const Vector3& scl,
			const Vector3& rot,
			const float length)
		{
			ObjMesh mesh;
			mesh.LoadPlane(pos, scl, rot, length);

			mpVertexBuf = CreateVertexBuffer(&mesh.GetVertexAt(0), mesh.GetVertexCount());
			mpIndexBuf = CreateIndexBuffer(mesh.GetIndexAt(0), mesh.GetTriangleCount());

			mTextures.push_back(new ConstantTexture<Color>(0.9f * Color::WHITE));
		}

		void Mesh::LoadSphere(const Vector3& pos,
			const Vector3& scl,
			const Vector3& rot,
			const float radius,
			const int slices,
			const int stacks)
		{
			ObjMesh mesh;
			mesh.LoadSphere(pos, scl, rot, radius, slices, stacks);

			mpVertexBuf = CreateVertexBuffer(&mesh.GetVertexAt(0), mesh.GetVertexCount());
			mpIndexBuf = CreateIndexBuffer(mesh.GetIndexAt(0), mesh.GetTriangleCount());

			mTextures.push_back(new ConstantTexture<Color>(0.9f * Color::WHITE));
		}
	}
}