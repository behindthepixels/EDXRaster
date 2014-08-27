#pragma once

#include "Graphics/ObjMesh.h"
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

		public:
			void LoadPlane(const Vector3& pos,
				const Vector3& scl,
				const Vector3& rot,
				const float length);

			const VertexBuffer<Vertex_PositionNormalTex>* GetVertexBuffer() const
			{
				return mpVertexBuf.Ptr();
			}
			const IndexBuffer* GetIndexBuffer() const
			{
				return mpIndexBuf.Ptr();
			}
		};
	}
}