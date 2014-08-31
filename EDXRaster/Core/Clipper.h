#pragma once

#include "EDXPrerequisites.h"
#include "Shader.h"
#include "../Utils/InputBuffer.h"

#define CLIP_ALL_PLANES 1

namespace EDX
{
	namespace RasterRenderer
	{
		struct Polygon
		{
			struct Vertex
			{
				Vector4 pos;
				Vector3 clipWeights;

				Vertex(const Vector4& p = Vector4::ZERO, const Vector3& w = Vector3::ZERO)
					: pos(p)
					, clipWeights(w)
				{
				}
			};

			vector<Vertex> vertices;
			void FromTriangle(const Vector4& v0, const Vector4& v1, const Vector4& v2)
			{
				vertices.resize(3);
				vertices[0].pos = v0; vertices[1].pos = v1; vertices[2].pos = v2;
				vertices[0].clipWeights = Vector3::UNIT_X;
				vertices[1].clipWeights = Vector3::UNIT_Y;
				vertices[2].clipWeights = Vector3::UNIT_Z;
			}
		};

		class Clipper
		{
		private:
			static const uint INSIDE_BIT = 0;
			static const uint LEFT_BIT = 1 << 0;
			static const uint RIGHT_BIT = 1 << 1;
			static const uint BOTTOM_BIT = 1 << 2;
			static const uint TOP_BIT = 1 << 3;
			static const uint FAR_BIT = 1 << 5;
			static const uint NEAR_BIT = 1 << 4;

			static uint ComputeClipCode(const Vector4& v)
			{
				uint code = INSIDE_BIT;

#if CLIP_ALL_PLANES
				if (v.x < -v.w)
					code |= LEFT_BIT;
				if (v.x > v.w)
					code |= RIGHT_BIT;
				if (v.y < -v.w)
					code |= BOTTOM_BIT;
				if (v.y > v.w)
					code |= TOP_BIT;
				if (v.z > v.w)
					code |= FAR_BIT;
#endif
				if (v.z < 0.0f)
					code |= NEAR_BIT;

				return code;
			}

		public:
			static void Clip(vector<ProjectedVertex>& verticesIn, IndexBuffer* pIndexBufOut, const IndexBuffer* pIndexBufSrc)
			{
				for (auto i = 0; i < pIndexBufSrc->GetTriangleCount(); i++)
				{
					const uint* pIndex = pIndexBufSrc->GetIndex(i);
					int idx0 = pIndex[0], idx1 = pIndex[1], idx2 = pIndex[2];
					const Vector4& v0 = verticesIn[idx0].projectedPos;
					const Vector4& v1 = verticesIn[idx1].projectedPos;
					const Vector4& v2 = verticesIn[idx2].projectedPos;

					uint clipCode0 = ComputeClipCode(v0);
					uint clipCode1 = ComputeClipCode(v1);
					uint clipCode2 = ComputeClipCode(v2);

					if ((clipCode0 | clipCode1 | clipCode2))
					{
						if (!(clipCode0 & clipCode1 & clipCode2))
						{
							int clipVertIds[12];

							Polygon polygon0, polygon1;
							polygon0.FromTriangle(v0, v1, v2);

							Polygon* pCurrPoly = &polygon0;
							Polygon* pbuffPoly = &polygon1;
							ClipPolygon(pCurrPoly, pbuffPoly,
								(clipCode0 ^ clipCode1) | (clipCode1 ^ clipCode2) | (clipCode2 ^ clipCode0));

							for (int j = 0; j < pCurrPoly->vertices.size(); j++)
							{
								Vector3 weight = pCurrPoly->vertices[j].clipWeights;
								if (weight.x == 1.0f)
								{
									clipVertIds[j] = idx0;
								}
								else if (weight.y == 1.0f)
								{
									clipVertIds[j] = idx1;
								}
								else if (weight.z == 1.0f)
								{
									clipVertIds[j] = idx2;
								}
								else
								{
									clipVertIds[j] = verticesIn.size();
									ProjectedVertex tmpVertex;
									tmpVertex.projectedPos = pCurrPoly->vertices[j].pos;
									tmpVertex.position = weight.x * verticesIn[idx0].position +
										weight.y * verticesIn[idx1].position +
										weight.z * verticesIn[idx2].position;
									tmpVertex.normal = weight.x * verticesIn[idx0].normal +
										weight.y * verticesIn[idx1].normal +
										weight.z * verticesIn[idx2].normal;
									tmpVertex.texCoord = weight.x * verticesIn[idx0].texCoord +
										weight.y * verticesIn[idx1].texCoord +
										weight.z * verticesIn[idx2].texCoord;

									verticesIn.push_back(tmpVertex);
								}
							}

							// Simple triangulation
							for (int k = 2; k < pCurrPoly->vertices.size(); k++)
							{
								pIndexBufOut->AppendTriangle(clipVertIds[0], clipVertIds[k - 1], clipVertIds[k]);
							}
						}

						continue;
					}

					pIndexBufOut->AppendTriangle(idx0, idx1, idx2);
				}
			}

		private:
			template<typename PredicateFunc, typename ComputeTFunc, typename ClipFunc>
			static void ClipByPlane(Polygon*& pInput, Polygon*& pBuffer, PredicateFunc predicate, ComputeTFunc computeT, ClipFunc clip)
			{
				pBuffer->vertices.clear();
				for (int i = 0; i < pInput->vertices.size(); i++)
				{
					int i1 = i + 1;
					if (i1 == pInput->vertices.size()) i1 = 0;
					Vector4 v0 = pInput->vertices[i].pos;
					Vector4 v1 = pInput->vertices[i1].pos;
					if (predicate(v0))
					{
						if (predicate(v1))
						{
							pBuffer->vertices.push_back(Polygon::Vertex(v1, pInput->vertices[i1].clipWeights));
						}
						else
						{
							float t = computeT(v0, v1);
							Vector4 pos = v0 * (1 - t) + v1 * t;
							clip(pos);
							Vector3 weight = pInput->vertices[i].clipWeights * (1 - t) + pInput->vertices[i1].clipWeights * t;
							pBuffer->vertices.push_back(Polygon::Vertex(pos, weight));
						}
					}
					else
					{
						if (predicate(v1))
						{
							float t = computeT(v0, v1);
							Vector4 pos = v0 * (1 - t) + v1 * t;
							clip(pos);
							Vector3 weight = pInput->vertices[i].clipWeights * (1 - t) + pInput->vertices[i1].clipWeights * t;
							pBuffer->vertices.push_back(Polygon::Vertex(pos, weight));
							pBuffer->vertices.push_back(Polygon::Vertex(v1, pInput->vertices[i1].clipWeights));
						}
					}
				}

				swap(pInput, pBuffer);
			}

		public:
			static void ClipPolygon(Polygon*& pInput, Polygon*& pBuffer, const uint planeCode)
			{
#if CLIP_ALL_PLANES
				if (planeCode & LEFT_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.x >= -v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (v0.w + v0.x) / ((v0.x + v0.w) - (v1.x + v1.w)); },
						[](Vector4& v) { v.x = -v.w; });
				}

				if (planeCode & RIGHT_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.x <= v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (-v0.w + v0.x) / ((v0.x - v0.w) - (v1.x - v1.w)); },
						[](Vector4& v) { v.x = v.w; });
				}

				if (planeCode & BOTTOM_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.y >= -v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (v0.w + v0.y) / ((v0.y + v0.w) - (v1.y + v1.w)); },
						[](Vector4& v) { v.y = -v.w; });
				}

				if (planeCode & TOP_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.y <= v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (-v0.w + v0.y) / ((v0.y - v0.w) - (v1.y - v1.w)); },
						[](Vector4& v) { v.y = v.w; });
				}

				if (planeCode & FAR_BIT)
				{
					ClipByPlane(pInput, pBuffer, [=](const Vector4& v) -> bool { return v.z <= v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (-v0.w + v0.z) / ((v0.z - v0.w) - (v1.z - v1.w)); },
						[=](Vector4& v) { v.z = v.w; });
				}
#endif
				if (planeCode & NEAR_BIT)
				{
					ClipByPlane(pInput, pBuffer, [=](const Vector4& v) -> bool { return v.z >= 0.0f; },
						[](const Vector4& v0, const Vector4& v1) -> float { return v0.z / (v0.z - v1.z); },
						[=](Vector4& v) { v.z = 0.0f; });
				}

				for (int i = 0; i<pInput->vertices.size(); i++)
				{
					if (pInput->vertices[i].pos.w <= 0.0f)
					{
						pInput->vertices.clear();
						return;
					}
				}
			}
		};
	}
}