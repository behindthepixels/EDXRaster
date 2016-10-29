#pragma once

#include "EDXPrerequisites.h"
#include "Core/Memory.h"

namespace EDX
{
	namespace RasterRenderer
	{
		enum class VertexFormat
		{
			PositionNormalTex,
			PositionNormalColor
		};

		struct Vertex_PositionNormalTex
		{
			Vector3 Position;
			Vector3 Normal;
			Vector2 TexCoord;
			static const VertexFormat Format = VertexFormat::PositionNormalTex;
			static const int Size = 32;

			static const int PosOffset = 0;
			static const int NormalOffset = 12;
			static const int TexOffset = 24;
			static const int ColorOffset = -1;
		};
		struct Vertex_PositionNormalColor
		{
			Vector3 Position;
			Vector3 Normal;
			Color	Color;
			static const VertexFormat Format = VertexFormat::PositionNormalColor;
			static const int Size = 32;

			static const int PosOffset = 0;
			static const int NormalOffset = 12;
			static const int TexOffset = -1;
			static const int ColorOffset = 24;
		};

		class IVertexBuffer
		{
		protected:
			size_t mBufSize;
			uint mVertexCount;

		public:
			virtual ~IVertexBuffer() {}

			virtual void		 NewBuffer(const uint vertexCount) = 0;
			virtual void*		 GetBuffer() const = 0;
			virtual VertexFormat GetVertexFormat() const = 0;
			virtual int			 GetVertexSize() const = 0;
			virtual size_t		 GetBufferSize() const = 0;
			virtual Vector3		 GetPosition(const uint idx) const = 0;
			virtual Vector3		 GetNormal(const uint idx) const = 0;
			virtual Vector2		 GetTexCoord(const uint idx) const = 0;
			virtual Color		 GetColor(const uint idx) const = 0;
			virtual void		 Release() = 0;
			uint				 GetVertexCount() const
			{
				return mVertexCount;
			}
		};

		template<typename VertexType = Vertex_PositionNormalTex>
		class VertexBuffer : public IVertexBuffer
		{
		private:
			_byte* mpBuffer;

		public:
			~VertexBuffer()
			{
				Release();
			}

			void NewBuffer(const uint vertexCount)
			{
				mVertexCount = vertexCount;
				mBufSize = vertexCount * VertexType::Size;

				mpBuffer = new _byte[mBufSize];
			}
			void* GetBuffer() const
			{
				return mpBuffer;
			}
			inline VertexFormat GetVertexFormat() const
			{
				return VertexType::Format;
			}
			inline int GetVertexSize() const
			{
				return VertexType::Size;
			}
			inline size_t GetBufferSize() const
			{
				return mBufSize;
			}
			inline Vector3 GetPosition(const uint idx) const
			{
				Vector3* ret = (Vector3*)(mpBuffer + idx * VertexType::Size);
				return *ret;
			}
			inline Vector3 GetNormal(const uint idx) const
			{
				if (VertexType::NormalOffset == -1)
					return Vector3::ZERO;
				Vector3* ret = (Vector3*)(mpBuffer + idx * VertexType::Size + VertexType::NormalOffset);
				return *ret;
			}
			inline Vector2 GetTexCoord(const uint idx) const
			{
				if (VertexType::TexOffset == -1)
					return Vector2::ZERO;
				Vector2* ret = (Vector2*)(mpBuffer + idx * VertexType::Size + VertexType::TexOffset);
				return *ret;
			}
			inline Color GetColor(const uint idx) const
			{
				if (VertexType::ColorOffset == -1)
					return Color::WHITE;
				Color* ret = (Color*)(mpBuffer + idx * VertexType::Size + VertexType::ColorOffset);
				return *ret;
			}
			void Release()
			{
				Memory::SafeDeleteArray(mpBuffer);
			}

		};

		template<typename VertexType = Vertex_PositionNormalTex>
		static IVertexBuffer* CreateVertexBuffer(const void* pData, const size_t vertexCount)
		{
			IVertexBuffer* ret = nullptr;
			ret = new VertexBuffer < VertexType >;

			ret->NewBuffer(vertexCount);
			memcpy(ret->GetBuffer(), pData, ret->GetBufferSize());

			return ret;
		}

		class IndexBuffer
		{
		private:
			Array<uint>	mBuffer;

		public:
			~IndexBuffer()
			{
				Release();
			}
			
			void ResizeBuffer(const uint triCount)
			{
				mBuffer.Resize(3 * triCount);
			}
			inline uint* GetBuffer()
			{
				return mBuffer.Data();
			}
			inline uint GetTriangleCount() const
			{
				return mBuffer.Size() / 3;
			}
			inline size_t GetBufferSize() const
			{
				return mBuffer.Size();
			}
			inline const uint* GetIndex(const uint idx) const
			{
				Assert(3 * idx < mBuffer.Size());
				return (uint*)&mBuffer[3 * idx];
			}
			inline void AppendTriangle(const int idx0, const int idx1, const int idx2)
			{
				mBuffer.Add(idx0);
				mBuffer.Add(idx1);
				mBuffer.Add(idx2);
			}
			inline void CopyFrom(IndexBuffer& other)
			{
				mBuffer.Resize(other.GetBufferSize());
				memcpy(mBuffer.Data(), other.GetBuffer(), other.GetBufferSize() * sizeof(uint));
			}
			void Release()
			{
			}
		};

		static IndexBuffer* CreateIndexBuffer(const void* pData, const size_t triCount)
		{
			IndexBuffer* ret = nullptr;
			ret = new IndexBuffer;

			ret->ResizeBuffer(triCount);
			memcpy(ret->GetBuffer(), pData, ret->GetBufferSize() * sizeof(uint));

			return ret;
		}

		template class VertexBuffer < Vertex_PositionNormalTex > ;
	}
}