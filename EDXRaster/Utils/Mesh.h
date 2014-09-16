#pragma once

#include "Graphics/ObjMesh.h"
#include "Graphics/Texture.h"

#include "Memory/RefPtr.h"

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
			RefPtr<VertexBuffer<struct Vertex_PositionNormalTex>> mpVertexBuf;
			RefPtr<IndexBuffer> mpIndexBuf;

			vector<RefPtr<Texture2D<Color>>> mTextures;
			vector<uint> mTexIdx;

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

			const VertexBuffer<Vertex_PositionNormalTex>* GetVertexBuffer() const
			{
				return mpVertexBuf.Ptr();
			}
			IndexBuffer* GetIndexBuffer() const
			{
				return mpIndexBuf.Ptr();
			}
			const vector<RefPtr<Texture2D<Color>>>& GetTextures() const
			{
				return mTextures;
			}
			const vector<uint>& GetTextureIds() const
			{
				return mTexIdx;
			}

			void Release();
		};
	}
}