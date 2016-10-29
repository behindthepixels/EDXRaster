#include "Mesh.h"
#include "InputBuffer.h"
#include "Graphics/ObjMesh.h"
#include "Core/Memory.h"


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

			mpVertexBuf.Reset(CreateVertexBuffer(&mesh.GetVertexAt(0), mesh.GetVertexCount()));
			mpIndexBuf.Reset(CreateIndexBuffer(mesh.GetIndexAt(0), mesh.GetTriangleCount()));

			// Initialize materials
			const auto& materialInfo = mesh.GetMaterialInfo();
			for (auto i = 0; i < materialInfo.Size(); i++)
			{
				if (materialInfo[i].strTexturePath[0])
					mTextures.Add(MakeUnique<ImageTexture<Color, Color4b>>(materialInfo[i].strTexturePath, 1.0f));
				else
					mTextures.Add(MakeUnique<ConstantTexture2D<Color>>(materialInfo[i].color));
			}
			mTexIdx = mesh.GetMaterialIdxBuf();

			mBounds = mesh.GetBounds();
		}

		void Mesh::LoadPlane(const Vector3& pos,
			const Vector3& scl,
			const Vector3& rot,
			const float length)
		{
			ObjMesh mesh;
			mesh.LoadPlane(pos, scl, rot, length);

			mpVertexBuf.Reset(CreateVertexBuffer(&mesh.GetVertexAt(0), mesh.GetVertexCount()));
			mpIndexBuf.Reset(CreateIndexBuffer(mesh.GetIndexAt(0), mesh.GetTriangleCount()));

			mTextures.Add(MakeUnique<ConstantTexture2D<Color>>(0.9f * Color::WHITE));
			mTexIdx = mesh.GetMaterialIdxBuf();

			mBounds = mesh.GetBounds();
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

			mpVertexBuf.Reset(CreateVertexBuffer(&mesh.GetVertexAt(0), mesh.GetVertexCount()));
			mpIndexBuf.Reset(CreateIndexBuffer(mesh.GetIndexAt(0), mesh.GetTriangleCount()));

			mTextures.Add(MakeUnique<ConstantTexture2D<Color>>(0.9f * Color::WHITE));
			mTexIdx = mesh.GetMaterialIdxBuf();

			mBounds = mesh.GetBounds();
		}

		void Mesh::Release()
		{
			mpVertexBuf.Reset(nullptr);
			mpIndexBuf.Reset(nullptr);
			mTextures.Clear();
			mTexIdx.Clear();
		}
	}
}