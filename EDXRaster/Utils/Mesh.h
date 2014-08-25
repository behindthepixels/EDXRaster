#pragma once

#include "Graphics/ObjMesh.h"
#include "Memory/RefPtr.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Mesh
		{
		private:
			RefPtr<class IVertexBuffer>	mpVertexBuf;
			RefPtr<class IndexBuffer> mpIndexBuf;

		public:
			void LoadPlane(const Vector3& pos,
				const Vector3& scl,
				const Vector3& rot,
				const float length);

			const IVertexBuffer* GetVertexBuffer() const
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