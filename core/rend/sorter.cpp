/*
	 This file is part of reicast.

	 reicast is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 2 of the License, or
	 (at your option) any later version.

	 reicast is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "sorter.h"

// Vulkan and DirectX use the color values of the first vertex for flat shaded triangle strips.
// On Dreamcast the last vertex is the provoking one so we must copy it onto the first.
void setFirstProvokingVertex(rend_context& rendContext)
{
	auto setProvokingVertex = [&rendContext](const List<PolyParam>& list) {
        u32 * const idx_base = rendContext.idx.head();
        Vertex * const vtx_base = rendContext.verts.head();
		for (const PolyParam& pp : list)
		{
			if (pp.pcw.Gouraud)
				continue;
			for (u32 i = 0; i + 2 < pp.count; i++)
			{
				u32 idx = idx_base[pp.first + i];
				if (idx == (u32)-1)
					// primitive restart
					continue;
				Vertex& vertex = vtx_base[idx];
				idx = idx_base[pp.first + i + 1];
				if (idx == (u32)-1) {
					i++;
					// primitive restart
					continue;
				}
				idx = idx_base[pp.first + i + 2];
				if (idx == (u32)-1) {
					i += 2;
					// primitive restart
					continue;
				}
				Vertex& lastVertex = vtx_base[idx];
				memcpy(vertex.col, lastVertex.col, sizeof(vertex.col));
				memcpy(vertex.spc, lastVertex.spc, sizeof(vertex.spc));
				memcpy(vertex.col1, lastVertex.col1, sizeof(vertex.col1));
				memcpy(vertex.spc1, lastVertex.spc1, sizeof(vertex.spc1));
			}
		}
	};
	setProvokingVertex(rendContext.global_param_op);
	setProvokingVertex(rendContext.global_param_pt);
	if (rendContext.sortedTriangles.empty())
	{
		setProvokingVertex(rendContext.global_param_tr);
	}
	else
	{
		Vertex * const vtx_base = rendContext.verts.head();
		u32 * const idx_base = rendContext.idx.head();
		for (const SortedTriangle& tri : rendContext.sortedTriangles)
		{
			if (tri.ppid->pcw.Gouraud)
				continue;
			for (u32 i = 0; i + 2 < tri.count; i += 3)
			{
				Vertex& vertex = vtx_base[idx_base[tri.first + i]];
				Vertex& lastVertex = vtx_base[idx_base[tri.first + i + 2]];
				memcpy(vertex.col, lastVertex.col, sizeof(vertex.col));
				memcpy(vertex.spc, lastVertex.spc, sizeof(vertex.spc));
				memcpy(vertex.col1, lastVertex.col1, sizeof(vertex.col1));
				memcpy(vertex.spc1, lastVertex.spc1, sizeof(vertex.spc1));
			}
		}
	}
}
