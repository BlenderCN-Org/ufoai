/**
 * @file r_model_md2.c
 * @brief md2 alias model loading
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"

#define MAX_LBM_HEIGHT 1024

/*
==============================================================================
MD2 ALIAS MODELS
==============================================================================
*/

static void R_ModLoadTags (model_t * mod, void *buffer, int bufSize)
{
	dMD2tag_t *pintag, *pheader;
	int version;
	int i, j, size;
	float *inmat, *outmat;
	int read;

	pintag = (dMD2tag_t *) buffer;

	version = LittleLong(pintag->version);
	if (version != TAG_VERSION)
		Sys_Error("R_ModLoadTags: %s has wrong version number (%i should be %i)", mod->alias.tagname, version, TAG_VERSION);

	size = LittleLong(pintag->ofs_extractend);
	mod->alias.tagdata = VID_TagAlloc(vid_modelPool, size, 0);
	pheader = mod->alias.tagdata;

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(dMD2tag_t) / 4; i++)
		((int *) pheader)[i] = LittleLong(((int *) buffer)[i]);

	if (pheader->num_tags <= 0)
		Sys_Error("R_ModLoadTags: tag file %s has no tags", mod->alias.tagname);

	if (pheader->num_frames <= 0)
		Sys_Error("R_ModLoadTags: tag file %s has no frames", mod->alias.tagname);

	/* load tag names */
	memcpy((char *) pheader + pheader->ofs_names, (char *) pintag + pheader->ofs_names, pheader->num_tags * MD2_MAX_SKINNAME);

	/* load tag matrices */
	inmat = (float *) ((byte *) pintag + pheader->ofs_tags);
	outmat = (float *) ((byte *) pheader + pheader->ofs_tags);

	if (bufSize != pheader->ofs_end)
		Sys_Error("R_ModLoadTags: tagfile %s is broken - expected: %i, offsets tell us to read: %i\n",
			mod->alias.tagname, bufSize, pheader->ofs_end);

	if (pheader->num_frames != mod->alias.num_frames)
		Com_Printf("R_ModLoadTags: found %i frames in %s but model has %i frames\n",
			pheader->num_frames, mod->alias.tagname, mod->alias.num_frames);

	if (pheader->ofs_names != 32)
		Sys_Error("R_ModLoadTags: invalid ofs_name for tagfile %s\n", mod->alias.tagname);
	if (pheader->ofs_tags != pheader->ofs_names + (pheader->num_tags * 64))
		Sys_Error("R_ModLoadTags: invalid ofs_tags for tagfile %s\n", mod->alias.tagname);
	/* (4 * 3) * 4 bytes (int) */
	if (pheader->ofs_end != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 48))
		Sys_Error("R_ModLoadTags: invalid ofs_end for tagfile %s\n", mod->alias.tagname);
	/* (4 * 4) * 4 bytes (int) */
	if (pheader->ofs_extractend != pheader->ofs_tags + (pheader->num_tags * pheader->num_frames * 64))
		Sys_Error("R_ModLoadTags: invalid ofs_extractend for tagfile %s\n", mod->alias.tagname);

	for (i = 0; i < pheader->num_tags * pheader->num_frames; i++) {
		for (j = 0; j < 4; j++) {
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = LittleFloat(*inmat++);
			*outmat++ = 0.0;
		}
		outmat--;
		*outmat++ = 1.0;
	}

	read = (byte *)outmat - (byte *)pheader;
	if (read != size)
		Sys_Error("R_ModLoadTags: read: %i expected: %i - tags: %i, frames: %i (should be %i)",
			read, size, pheader->num_tags, pheader->num_frames, mod->alias.num_frames);
}

/**
 * @brief Load MD2 models from file.
 */
void R_ModLoadAliasMD2Model (model_t *mod, void *buffer, int bufSize)
{
	int i, j;
	dMD2Model_t *md2;
	dMD2Triangle_t *pintri;
	dMD2Coord_t *pincoord;
	dMD2Frame_t *pinframe;
	int version, size;
	byte *tagbuf = NULL, *animbuf = NULL;
	mAliasMesh_t *outMesh;
	mAliasFrame_t *outFrame;
	mAliasVertex_t *outVertex;
	mAliasCoord_t *outCoord;
	int32_t tempIndex[MD2_MAX_TRIANGLES * 3];
	int32_t tempSTIndex[MD2_MAX_TRIANGLES * 3];
	int indRemap[MD2_MAX_TRIANGLES * 3];
	int32_t *outIndex;
	int frameSize, numIndexes, numVerts, skinWidth, skinHeight;
	double isw, ish;
	size_t l;

	/* fixed values */
	mod->type = mod_alias_md2;
	mod->alias.num_meshes = 1;

	/* get the disk data */
	md2 = (dMD2Model_t *) buffer;

	/* sanity checks */
	version = LittleLong(md2->version);
	if (version != MD2_ALIAS_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, MD2_ALIAS_VERSION);

	if (bufSize != LittleLong(md2->ofs_end))
		Sys_Error("model %s broken offset values", mod->name);

	skinHeight = LittleLong(md2->skinheight);
	skinWidth = LittleLong(md2->skinwidth);
	if (skinHeight <= 0 || skinWidth <= 0)
		Sys_Error("model %s has invalid skin dimensions '%d x %d'", mod->name, skinHeight, skinWidth);

	/* only one mesh for md2 models */
	mod->alias.num_frames = LittleLong(md2->num_frames);
	if (mod->alias.num_frames <= 0 || mod->alias.num_frames >= MD2_MAX_FRAMES)
		Sys_Error("model %s has too many (or no) frames", mod->name);

	mod->alias.meshes = outMesh = VID_TagAlloc(vid_modelPool, sizeof(mAliasMesh_t), 0);
	outMesh->num_verts = LittleLong(md2->num_verts);
	if (outMesh->num_verts <= 0 || outMesh->num_verts >= MD2_MAX_VERTS)
		Sys_Error("model %s has too many (or no) vertices", mod->name);
	outMesh->num_tris = LittleLong(md2->num_tris);
	if (outMesh->num_tris <= 0 || outMesh->num_tris >= MD2_MAX_TRIANGLES)
		Sys_Error("model %s has too many (or no) triangles", mod->name);
	frameSize = LittleLong(md2->framesize);

	/* load the skins */
	outMesh->num_skins = LittleLong(md2->num_skins);
	if (outMesh->num_skins < 0 || outMesh->num_skins >= MD2_MAX_SKINS) {
		Com_Printf("Could not load model '%s' - invalid num_skins value: %i\n", mod->name, outMesh->num_skins);
		return;
	}
	outMesh->skins = VID_TagAlloc(vid_modelPool, sizeof(mAliasSkin_t) * outMesh->num_skins, 0);
	for (i = 0; i < outMesh->num_skins; i++) {
		outMesh->skins[i].skin = R_AliasModelGetSkin(mod,
			(char *) md2 + md2->ofs_skins + i * MD2_MAX_SKINNAME);
		Q_strncpyz(outMesh->skins[i].name, outMesh->skins[i].skin->name, sizeof(outMesh->skins[i].name));
	}
	outMesh->skinWidth = skinWidth;
	outMesh->skinHeight = skinHeight;

	isw = 1.0 / (double)outMesh->skinWidth;
	ish = 1.0 / (double)outMesh->skinHeight;

	/* load triangle lists */
	pintri = (dMD2Triangle_t *) ((byte *) md2 + LittleLong(md2->ofs_tris));
	pincoord = (dMD2Coord_t *) ((byte *) md2 + LittleLong(md2->ofs_st));

	for (i = 0; i < outMesh->num_tris; i++) {
		for (j = 0; j < 3; j++) {
			tempIndex[i * 3 + j] = (int32_t)LittleShort(pintri[i].index_verts[j]);
			tempSTIndex[i * 3 + j] = (int32_t)LittleShort(pintri[i].index_st[j]);
		}
	}

	/* build list of unique vertices */
	numIndexes = outMesh->num_tris * 3;
	numVerts = 0;
	outMesh->indexes = outIndex = VID_TagAlloc(vid_modelPool, sizeof(int32_t) * numIndexes, 0);

	for (i = 0; i < numIndexes; i++)
		indRemap[i] = -1;

	for (i = 0; i < numIndexes; i++) {
		if (indRemap[i] != -1)
			continue;

		/* remap duplicates */
		for (j = i + 1; j < numIndexes; j++) {
			if (tempIndex[j] != tempIndex[i])
				continue;
			if (pincoord[tempSTIndex[j]].s != pincoord[tempSTIndex[i]].s
			 || pincoord[tempSTIndex[j]].t != pincoord[tempSTIndex[i]].t)
				continue;

			indRemap[j] = i;
			outIndex[j] = numVerts;
		}

		/* add unique vertex */
		indRemap[i] = i;
		outIndex[i] = numVerts++;
	}
	outMesh->num_verts = numVerts;
	if (outMesh->num_verts <= 0 || outMesh->num_verts >= 4096) {
		Com_Printf("R_ModLoadAliasMD2Model: invalid about of verts for model '%s'\n", mod->name);
		return;
	}

	for (i = 0; i < numIndexes; i++) {
		if (indRemap[i] == i)
			continue;

		outIndex[i] = outIndex[indRemap[i]];
	}

	outMesh->stcoords = outCoord = VID_TagAlloc(vid_modelPool, sizeof(mAliasCoord_t) * outMesh->num_verts, 0);
	for (j = 0; j < numIndexes; j++) {
		outCoord[outIndex[j]][0] = (float)(((double)LittleShort(pincoord[tempSTIndex[indRemap[j]]].s) + 0.5) * isw);
		outCoord[outIndex[j]][1] = (float)(((double)LittleShort(pincoord[tempSTIndex[indRemap[j]]].t) + 0.5) * isw);
	}

	/* load the frames */
	mod->alias.frames = outFrame = VID_TagAlloc(vid_modelPool, sizeof(mAliasFrame_t) * mod->alias.num_frames, 0);
	outMesh->vertexes = outVertex = VID_TagAlloc(vid_modelPool, sizeof(mAliasVertex_t) * mod->alias.num_frames * outMesh->num_verts, 0);

	for (i = 0; i < mod->alias.num_frames; i++) {
		pinframe = (dMD2Frame_t *) ((byte *) md2 + LittleLong(md2->ofs_frames) + i * frameSize);

		for (j = 0; j < 3; j++) {
			outFrame->scale[j] = LittleFloat(pinframe->scale[j]);
			outFrame->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		VectorCopy(outFrame->translate, outFrame->mins);
		VectorMA(outFrame->translate, 255, outFrame->scale, outFrame->maxs);

		AddPointToBounds(outFrame->mins, mod->mins, mod->maxs);
		AddPointToBounds(outFrame->maxs, mod->mins, mod->maxs);

		for (j = 0; j < numIndexes; j++) {
			outVertex[outIndex[j]].point[0] = (int16_t)pinframe->verts[tempIndex[indRemap[j]]].v[0] * outFrame->scale[0];
			outVertex[outIndex[j]].point[1] = (int16_t)pinframe->verts[tempIndex[indRemap[j]]].v[1] * outFrame->scale[1];
			outVertex[outIndex[j]].point[2] = (int16_t)pinframe->verts[tempIndex[indRemap[j]]].v[2] * outFrame->scale[2];
		}
	}

	/* load the tags */
	Q_strncpyz(mod->alias.tagname, mod->name, sizeof(mod->alias.tagname));
	/* strip model extension and set the extension to tag */
	l = strlen(mod->alias.tagname) - 4;
	strcpy(&(mod->alias.tagname[l]), ".tag");

	/* try to load the tag file */
	if (FS_CheckFile(mod->alias.tagname) != -1) {
		/* load the tags */
		size = FS_LoadFile(mod->alias.tagname, &tagbuf);
		R_ModLoadTags(mod, tagbuf, size);
		FS_FreeFile(tagbuf);
	}

	/* load the animations */
	Q_strncpyz(mod->alias.animname, mod->name, sizeof(mod->alias.animname));
	l = strlen(mod->alias.animname) - 4;
	strcpy(&(mod->alias.animname[l]), ".anm");

	/* try to load the animation file */
	if (FS_CheckFile(mod->alias.animname) != -1) {
		/* load the tags */
		FS_LoadFile(mod->alias.animname, &animbuf);
		R_ModLoadAnims(&mod->alias, animbuf);
		FS_FreeFile(animbuf);
	}
}
