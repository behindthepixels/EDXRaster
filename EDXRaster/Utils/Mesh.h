#pragma once

#include "Graphics/ObjMesh.h"
#include "Graphics/Texture.h"
#include "Math/BoundingBox.h"

#include "Core/SmartPointer.h"

namespace EDX
{
	namespace RasterRenderer
	{
		template<typename T>
		class VertexBuffer;
		class IndexBuffer;

		class Mesh
		{
		private:
			UniquePtr<class IVertexBuffer> mpVertexBuf;
			UniquePtr<IndexBuffer> mpIndexBuf;

			Array<UniquePtr<Texture2D<Color>>> mTextures;
			Array<uint> mTexIdx;

			BoundingBox mBounds;

		public:
			void LoadMesh(const Vector3& pos,
				const Vector3& scl,
				const Vector3& rot,
				const char* path);

			void LoadPlane(const Vector3& pos,
				const Vector3& scl,
				const Vector3& rot,
				const float length);

			void LoadSphere(const Vector3& pos,
				const Vector3& scl,
				const Vector3& rot,
				const float radius,
				const int slices = 64,
				const int stacks = 64);

			const IVertexBuffer* GetVertexBuffer() const
			{
				return mpVertexBuf.Get();
			}
			IndexBuffer* GetIndexBuffer() const
			{
				return mpIndexBuf.Get();
			}
			const Array<UniquePtr<Texture2D<Color>>>& GetTextures() const
			{
				return mTextures;
			}
			const Array<uint>& GetTextureIds() const
			{
				return mTexIdx;
			}

			inline const BoundingBox GetBounds() const
			{
				return mBounds;
			}

			void Release();
		};
	}
}