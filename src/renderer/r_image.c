/**
 * @file r_image.c
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
#include "r_error.h"

/* Workaround for a warning about redeclaring the macro. jpeglib sets this macro
 * and SDL, too. */
#undef HAVE_STDLIB_H

#include <jpeglib.h>
#include <png.h>

image_t r_images[MAX_GL_TEXTURES];
int r_numImages;
int registration_sequence;

/* generic environment map */
image_t *r_envmaptextures[MAX_ENVMAPTEXTURES];

/**
 * @brief Free previously loaded materials and their stages
 * @sa R_LoadMaterials
 */
void R_ImageClearMaterials (void)
{
	image_t *image;
	int i;

	/* clear previously loaded materials */
	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		material_t *m = &image->material;
		materialStage_t *s = m->stages;

		while (s) {  /* free the stages chain */
			materialStage_t *ss = s->next;
			Mem_Free(s);
			s = ss;
		}

		memset(m, 0, sizeof(*m));
		m->bump = m->specular = 1.0;
	}
}

/**
 * @brief Shows all loaded images
 */
void R_ImageList_f (void)
{
	int i;
	image_t *image;
	int texels;

	Com_Printf("------------------\n");
	texels = 0;

	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (!image->texnum)
			continue;
		texels += image->upload_width * image->upload_height;
		switch (image->type) {
		case it_effect:
			Com_Printf("EF");
			break;
		case it_skin:
			Com_Printf("SK");
			break;
		case it_wrappic:
			Com_Printf("WR");
			break;
		case it_chars:
			Com_Printf("CH");
			break;
		case it_static:
			Com_Printf("ST");
			break;
		case it_normalmap:
			Com_Printf("NM");
			break;
		case it_material:
			Com_Printf("MA");
			break;
		case it_lightmap:
			Com_Printf("LM");
			break;
		case it_world:
			Com_Printf("WO");
			break;
		case it_pic:
			Com_Printf("PI");
			break;
		default:
			Com_Printf("  ");
			break;
		}

		Com_Printf(" %3i %3i RGB: %5i idx: %i - %s\n", image->upload_width, image->upload_height, image->texnum, image->index, image->name);
	}
	Com_Printf("Total textures: %i (max textures: %i)\n", r_numImages, MAX_GL_TEXTURES);
	Com_Printf("Total texel count (not counting mipmaps): %i\n", texels);
}

/*
==============================================================================
PNG LOADING
==============================================================================
*/

typedef struct pngBuf_s {
	byte	*buffer;
	size_t	pos;
} pngBuf_t;

static void PngReadFunc (png_struct *Png, png_bytep buf, png_size_t size)
{
	pngBuf_t *PngFileBuffer = (pngBuf_t*)png_get_io_ptr(Png);
	memcpy(buf,PngFileBuffer->buffer + PngFileBuffer->pos, size);
	PngFileBuffer->pos += size;
}

/**
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_FindImage
 */
static int R_LoadPNG (const char *name, byte **pic, int *width, int *height)
{
	int rowptr;
	int samples, color_type, bit_depth;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	byte **row_pointers;
	byte *img;
	uint32_t i;
	pngBuf_t PngFileBuffer = {NULL, 0};

	if (*pic != NULL)
		Sys_Error("possible mem leak in LoadPNG\n");

	/* Load the file */
	FS_LoadFile(name, (byte **)&PngFileBuffer.buffer);
	if (!PngFileBuffer.buffer)
		return 0;

	/* Parse the PNG file */
	if ((png_check_sig(PngFileBuffer.buffer, 8)) == 0) {
		Com_Printf("LoadPNG: Not a PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
	}

	PngFileBuffer.pos = 0;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
	}

	end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		Com_Printf("LoadPNG: Bad PNG file: %s\n", name);
		FS_FreeFile(PngFileBuffer.buffer);
		return 0;
	}

	/* get some usefull information from header */
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);

	/**
	 * we want to treat all images the same way
	 * The following code transforms grayscale images of less than 8 to 8 bits,
	 * changes paletted images to RGB, and adds a full alpha channel if there is
	 * transparency information in a tRNS chunk.
	 */

	/* convert index color images to RGB images */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);
	/* convert 1-2-4 bits grayscale images to 8 bits grayscale */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);
	else if (bit_depth < 8)
		png_set_packing(png_ptr);

	png_set_read_fn(png_ptr, (png_voidp)&PngFileBuffer, (png_rw_ptr)PngReadFunc);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	row_pointers = png_get_rows(png_ptr, info_ptr);
	rowptr = 0;

	img = Mem_PoolAlloc(info_ptr->width * info_ptr->height * 4, vid_imagePool, 0);
	if (pic)
		*pic = img;

	if (info_ptr->channels == 4) {
		for (i = 0; i < info_ptr->height; i++) {
			memcpy(img + rowptr, row_pointers[i], info_ptr->rowbytes);
			rowptr += info_ptr->rowbytes;
		}
	} else {
		uint32_t j;

		memset(img, 255, info_ptr->width * info_ptr->height * 4);
		for (rowptr = 0, i = 0; i < info_ptr->height; i++) {
			for (j = 0; j < info_ptr->rowbytes; j += info_ptr->channels) {
				memcpy(img + rowptr, row_pointers[i] + j, info_ptr->channels);
				rowptr += 4;
			}
		}
	}

	if (width)
		*width = info_ptr->width;
	if (height)
		*height = info_ptr->height;
	samples = info_ptr->channels;

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	FS_FreeFile(PngFileBuffer.buffer);
	return samples;
}

/**
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_FindImage
 */
void R_WritePNG (qFILE *f, byte *buffer, int width, int height)
{
	int			i;
	png_structp	png_ptr;
	png_infop	info_ptr;
	png_bytep	*row_pointers;

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		Com_Printf("R_WritePNG: LibPNG Error!\n");
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		Com_Printf("R_WritePNG: LibPNG Error!\n");
		return;
	}

	png_init_io(png_ptr, f->f);

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 9);

	png_write_info(png_ptr, info_ptr);

	row_pointers = Mem_PoolAlloc(height * sizeof(png_bytep), vid_imagePool, 0);
	for (i = 0; i < height; i++)
		row_pointers[i] = buffer + (height - 1 - i) * 3 * width;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	Mem_Free(row_pointers);
}

/*
=========================================================
TARGA LOADING
=========================================================
*/

typedef struct targaHeader_s {
	byte idLength;
	byte colorMapType;
	byte imageType;

	uint16_t colorMapIndex;
	uint16_t colorMapLength;
	byte colorMapSize;

	uint16_t xOrigin;
	uint16_t yOrigin;
	uint16_t width;
	uint16_t height;

	byte pixelSize;

	byte attributes;
} targaHeader_t;

#define TGA_COLMAP_UNCOMP		1
#define TGA_COLMAP_COMP			9
#define TGA_UNMAP_UNCOMP		2
#define TGA_UNMAP_COMP			10
#define TGA_GREY_UNCOMP			3
#define TGA_GREY_COMP			11

/**
 * @sa R_LoadJPG
 * @sa R_LoadPNG
 * @sa R_FindImage
 */
void R_LoadTGA (const char *name, byte ** pic, int *width, int *height)
{
	int i, columns, rows, row_inc, row, col;
	byte *buf_p, *buffer, *pixbuf, *targaRGBA;
	int length, samples, readPixelCount, pixelCount;
	byte palette[256][4], red, green, blue, alpha;
	qboolean compressed;
	targaHeader_t targaHeader;

	if (*pic != NULL)
		Sys_Error("R_LoadTGA: possible mem leak\n");

	/* Load the file */
	length = FS_LoadFile(name, &buffer);
	if (!buffer || length <= 0) {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadTGA: Bad tga file %s\n", name);
		return;
	}

	/* Parse the header */
	buf_p = buffer;
	targaHeader.idLength = *buf_p++;
	targaHeader.colorMapType = *buf_p++;
	targaHeader.imageType = *buf_p++;

	targaHeader.colorMapIndex = buf_p[0] + buf_p[1] * 256; buf_p += 2;
	targaHeader.colorMapLength = buf_p[0] + buf_p[1] * 256; buf_p += 2;
	targaHeader.colorMapSize = *buf_p++;
	targaHeader.xOrigin = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.yOrigin = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.width = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.height = LittleShort(*((int16_t *)buf_p)); buf_p += 2;
	targaHeader.pixelSize = *buf_p++;
	targaHeader.attributes = *buf_p++;

	/* Skip TARGA image comment */
	if (targaHeader.idLength != 0)
		buf_p += targaHeader.idLength;

	compressed = qfalse;
	switch (targaHeader.imageType) {
	case TGA_COLMAP_COMP:
		compressed = qtrue;
	case TGA_COLMAP_UNCOMP:
		/* Uncompressed colormapped image */
		if (targaHeader.pixelSize != 8) {
			Com_Printf("R_LoadTGA: Only 8 bit images supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		if (targaHeader.colorMapLength != 256) {
			Com_Printf("R_LoadTGA: Only 8 bit colormaps are supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		if (targaHeader.colorMapIndex) {
			Com_Printf("R_LoadTGA: colorMapIndex is not supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}

		switch (targaHeader.colorMapSize) {
		case 32:
			for (i = 0; i < targaHeader.colorMapLength ; i++) {
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
			break;

		case 24:
			for (i = 0; i < targaHeader.colorMapLength ; i++) {
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = 255;
			}
			break;

		default:
			Com_Printf("R_LoadTGA: only 24 and 32 bit colormaps are supported for type 1 and 9 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;

	case TGA_UNMAP_COMP:
		compressed = qtrue;
	case TGA_UNMAP_UNCOMP:
		/* Uncompressed or RLE compressed RGB */
		if (targaHeader.pixelSize != 32 && targaHeader.pixelSize != 24) {
			Com_Printf("R_LoadTGA: Only 32 or 24 bit images supported for type 2 and 10 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;

	case TGA_GREY_COMP:
		compressed = qtrue;
	case TGA_GREY_UNCOMP:
		/* Uncompressed greyscale */
		if (targaHeader.pixelSize != 8) {
			Com_Printf("R_LoadTGA: Only 8 bit images supported for type 3 and 11 (%s)\n", name);
			FS_FreeFile(buffer);
			return;
		}
		break;
	default:
		Com_Printf("R_LoadTGA: Unknown tga image type: %i for image %s\n", targaHeader.imageType, name);
		FS_FreeFile(buffer);
		return;
	}

	columns = targaHeader.width;
	if (width)
		*width = columns;

	rows = targaHeader.height;
	if (height)
		*height = rows;

	targaRGBA = Mem_PoolAlloc(columns * rows * 4, vid_imagePool, 0);
	*pic = targaRGBA;

	/* If bit 5 of attributes isn't set, the image has been stored from bottom to top */
	if (targaHeader.attributes & 0x20) {
		pixbuf = targaRGBA;
		row_inc = 0;
	} else {
		pixbuf = targaRGBA + (rows - 1) * columns * 4;
		row_inc = -columns * 4 * 2;
	}

	red = blue = green = alpha = 0;

	for (row = col = 0, samples = 3; row < rows;) {
		pixelCount = 0x10000;
		readPixelCount = 0x10000;

		if (compressed) {
			pixelCount = *buf_p++;
			if (pixelCount & 0x80)	/* Run-length packet */
				readPixelCount = 1;
			pixelCount = 1 + (pixelCount & 0x7f);
		}

		while (pixelCount-- && row < rows) {
			if (readPixelCount-- > 0) {
				switch (targaHeader.imageType) {
				case TGA_COLMAP_UNCOMP:
				case TGA_COLMAP_COMP:
					/* Colormapped image */
					blue = *buf_p++;
					red = palette[blue][0];
					green = palette[blue][1];
					alpha = palette[blue][3];
					blue = palette[blue][2];
					if (alpha != 255)
						samples = 4;
					break;
				case TGA_UNMAP_UNCOMP:
				case TGA_UNMAP_COMP:
					/* 24 or 32 bit image */
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					if (targaHeader.pixelSize == 32) {
						alpha = *buf_p++;
						if (alpha != 255)
							samples = 4;
					} else
						alpha = 255;
					break;
				case TGA_GREY_UNCOMP:
				case TGA_GREY_COMP:
					/* Greyscale image */
					blue = green = red = *buf_p++;
					alpha = 255;
					break;
				default:
					Sys_Error("R_LoadTGA: Unknown tga image type: %i\n", targaHeader.imageType);
				}
			}

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			if (++col == columns) {
				/* Run spans across rows */
				row++;
				col = 0;
				pixbuf += row_inc;
			}
		}
	}

	FS_FreeFile(buffer);
}


/**
 * @sa R_LoadTGA
 * @sa R_WriteCompressedTGA
 */
void R_WriteTGA (qFILE *f, byte *buffer, int width, int height)
{
	int i, temp;
	const int channels = 3;
	byte *out;
	size_t size;

	/* Allocate an output buffer */
	size = (width * height * channels) + 18;
	out = Mem_PoolAlloc(size, vid_imagePool, 0);

	/* Fill in header */
	/* byte 0: image ID field length */
	/* byte 1: color map type */
	out[2] = TGA_UNMAP_UNCOMP;		/* image type: Uncompressed type */
	/* byte 3 - 11: palette data */
	/* image width */
	out[12] = width & 255;
	out[13] = (width >> 8) & 255;
	/* image height */
	out[14] = height & 255;
	out[15] = (height >> 8) & 255;
	out[16] = channels * 8;	/* Pixel size 24 (RGB) or 32 (RGBA) */
	/* byte 17: image descriptor byte */

	/* Copy to temporary buffer */
	memcpy(out + 18, buffer, width * height * channels);

	/* Swap rgb to bgr */
	for (i = 18; i < size; i += channels) {
		temp = out[i];
		out[i] = out[i+2];
		out[i + 2] = temp;
	}

	if (FS_Write(out, size, f) != size)
		Com_Printf("R_WriteTGA: Failed to write the tga file\n");

	Mem_Free(out);
}

#define TGA_CHANNELS 3

/**
 * @sa R_LoadTGA
 * @sa R_WriteTGA
 */
void R_WriteCompressedTGA (qFILE *f, byte *buffer, int width, int height)
{
	byte pixel_data[TGA_CHANNELS];
	byte block_data[TGA_CHANNELS * 128];
	byte rle_packet;
	int compress = 0;
	size_t block_length = 0;
	byte header[18];
	char footer[26];

	int y;
	size_t x;

	memset(header, 0, sizeof(header));
	memset(footer, 0, sizeof(footer));

	/* Fill in header */
	/* byte 0: image ID field length */
	/* byte 1: color map type */
	header[2] = TGA_UNMAP_COMP;		/* image type: Truecolor RLE encoded */
	/* byte 3 - 11: palette data */
	/* image width */
	header[12] = width & 255;
	header[13] = (width >> 8) & 255;
	/* image height */
	header[14] = height & 255;
	header[15] = (height >> 8) & 255;
	header[16] = 24;	/* Pixel size 24 (RGB) or 32 (RGBA) */
	header[17] = 0x20;	/* Origin at bottom left */

	/* write header */
	FS_Write(header, sizeof(header), f);

	for (y = height - 1; y >= 0; y--) {
		for (x = 0; x < width; x++) {
			const size_t index = y * width * TGA_CHANNELS + x * TGA_CHANNELS;
			pixel_data[0] = buffer[index + 2];
			pixel_data[1] = buffer[index + 1];
			pixel_data[2] = buffer[index];

			if (block_length == 0) {
				memcpy(block_data, pixel_data, TGA_CHANNELS);
				block_length++;
				compress = 0;
			} else {
				if (!compress) {
					/* uncompressed block and pixel_data differs from the last pixel */
					if (memcmp(&block_data[(block_length - 1) * TGA_CHANNELS], pixel_data, TGA_CHANNELS) != 0) {
						/* append pixel */
						memcpy(&block_data[block_length * TGA_CHANNELS], pixel_data, TGA_CHANNELS);

						block_length++;
					} else {
						/* uncompressed block and pixel data is identical */
						if (block_length > 1) {
							/* write the uncompressed block */
							rle_packet = block_length - 2;
							FS_Write(&rle_packet,1, f);
							FS_Write(block_data, (block_length - 1) * TGA_CHANNELS, f);
							block_length = 1;
						}
						memcpy(block_data, pixel_data, TGA_CHANNELS);
						block_length++;
						compress = 1;
					}
				} else {
					/* compressed block and pixel data is identical */
					if (memcmp(block_data, pixel_data, TGA_CHANNELS) == 0) {
						block_length++;
					} else {
						/* compressed block and pixel data differs */
						if (block_length > 1) {
							/* write the compressed block */
							rle_packet = block_length + 127;
							FS_Write(&rle_packet, 1, f);
							FS_Write(block_data, TGA_CHANNELS, f);
							block_length = 0;
						}
						memcpy(&block_data[block_length * TGA_CHANNELS], pixel_data, TGA_CHANNELS);
						block_length++;
						compress = 0;
					}
				}
			}

			if (block_length == 128) {
				rle_packet = block_length - 1;
				if (!compress) {
					FS_Write(&rle_packet, 1, f);
					FS_Write(block_data, 128 * TGA_CHANNELS, f);
				} else {
					rle_packet += 128;
					FS_Write(&rle_packet, 1, f);
					FS_Write(block_data, TGA_CHANNELS, f);
				}

				block_length = 0;
				compress = 0;
			}
		}
	}

	/* write remaining bytes */
	if (block_length) {
		rle_packet = block_length - 1;
		if (!compress) {
			FS_Write(&rle_packet, 1, f);
			FS_Write(block_data, block_length * TGA_CHANNELS, f);
		} else {
			rle_packet += 128;
			FS_Write(&rle_packet, 1, f);
			FS_Write(block_data, TGA_CHANNELS, f);
		}
	}

	/* write footer (optional, but the specification recommends it) */
	strncpy(&footer[8] , "TRUEVISION-XFILE", 16);
	footer[24] = '.';
	footer[25] = 0;
	FS_Write(footer, sizeof(footer), f);
}

/*
=================================================================
JPEG LOADING
By Robert 'Heffo' Heffernan
=================================================================
*/

static void jpg_null (j_decompress_ptr cinfo)
{
}

static boolean jpg_fill_input_buffer (j_decompress_ptr cinfo)
{
	Com_Printf("Premature end of JPEG data\n");
	return 1;
}

static void jpg_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	if (cinfo->src->bytes_in_buffer < (size_t) num_bytes)
		Com_Printf("Premature end of JPEG data\n");

	cinfo->src->next_input_byte += (size_t) num_bytes;
	cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void jpeg_mem_src (j_decompress_ptr cinfo, byte * mem, int len)
{
	cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
	cinfo->src->init_source = jpg_null;
	cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
	cinfo->src->skip_input_data = jpg_skip_input_data;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart;
	cinfo->src->term_source = jpg_null;
	cinfo->src->bytes_in_buffer = len;
	cinfo->src->next_input_byte = mem;
}

/**
 * @sa R_LoadTGA
 * @sa R_LoadPNG
 * @sa R_FindImage
 */
static void R_LoadJPG (const char *filename, byte ** pic, int *width, int *height)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *rawdata, *rgbadata, *scanline, *p, *q;
	int rawsize, i, components;

	if (*pic != NULL)
		Sys_Error("possible mem leak in LoadJPG\n");

	/* Load JPEG file into memory */
	rawsize = FS_LoadFile(filename, &rawdata);

	if (!rawdata)
		return;

	/* Knightmare- check for bad data */
	if (rawdata[6] != 'J' || rawdata[7] != 'F' || rawdata[8] != 'I' || rawdata[9] != 'F') {
		Com_Printf("Bad jpg file %s\n", filename);
		FS_FreeFile(rawdata);
		return;
	}

	/* Initialise libJpeg Object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	/* Feed JPEG memory into the libJpeg Object */
	jpeg_mem_src(&cinfo, rawdata, rawsize);

	/* Process JPEG header */
	jpeg_read_header(&cinfo, qtrue);

	/* Start Decompression */
	jpeg_start_decompress(&cinfo);

	components = cinfo.output_components;
    if (components != 3 && components != 1) {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadJPG: Bad jpeg components '%s' (%d)\n", filename, components);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	/* Check Colour Components */
	if (cinfo.output_components != 3 && cinfo.output_components != 4) {
		Com_Printf("Invalid JPEG colour components\n");
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	/* Allocate Memory for decompressed image */
	rgbadata = Mem_PoolAlloc(cinfo.output_width * cinfo.output_height * 4, vid_imagePool, 0);
	if (!rgbadata) {
		Com_Printf("Insufficient RAM for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}
	/* Allocate Scanline buffer */
	scanline = Mem_PoolAlloc(cinfo.output_width * components, vid_imagePool, 0);
	if (!scanline) {
		Com_Printf("Insufficient RAM for JPEG scanline buffer\n");
		Mem_Free(rgbadata);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	/* Pass sizes to output */
	*width = cinfo.output_width;
	*height = cinfo.output_height;

	/* Read Scanlines, and expand from RGB to RGBA */
	q = rgbadata;
	while (cinfo.output_scanline < cinfo.output_height) {
		p = scanline;
		jpeg_read_scanlines(&cinfo, &scanline, 1);

		if (components == 1) {
			for (i = 0; i < cinfo.output_width; i++, q += 4) {
				q[0] = q[1] = q[2] = *p++;
				q[3] = 255;
			}
		} else {
			for (i = 0; i < cinfo.output_width; i++, q += 4, p += 3) {
				q[0] = p[0], q[1] = p[1], q[2] = p[2];
				q[3] = 255;
			}
		}
	}

	/* Free the scanline buffer */
	Mem_Free(scanline);

	/* Finish Decompression */
	jpeg_finish_decompress(&cinfo);

	/* Destroy JPEG object */
	jpeg_destroy_decompress(&cinfo);

	/* Return the 'rgbadata' */
	*pic = rgbadata;
	FS_FreeFile(rawdata);
}

/**
 * @brief Generic image-data loading fucntion.
 * @param[in] name (Full) pathname to the image to load. Extension (if given) will be ignored.
 * @param[out] pic Image data.
 * @param[out] width Width of the loaded image.
 * @param[out] height Height of the loaded image.
 * @sa R_FindImage
 */
void R_LoadImage (const char *name, byte **pic, int *width, int *height)
{
	char filename_temp[MAX_QPATH];
	char *ename;
	int len;

	if (!name)
		Sys_Error("R_LoadImage: NULL name");
	len = strlen(name);

	if (len >= 5) {
		/* Drop extension */
		Q_strncpyz(filename_temp, name, MAX_QPATH);
		if (filename_temp[len - 4] == '.')
			len -= 4;
	}

	ename = &(filename_temp[len]);
	*ename = 0;

	/* Check if there is any image at this path. */

	strcpy(ename, ".tga");
	if (FS_CheckFile(filename_temp) != -1) {
		R_LoadTGA(filename_temp, pic, width, height);
		if (pic)
			return;
	}

	strcpy(ename, ".jpg");
	if (FS_CheckFile(filename_temp) != -1) {
		R_LoadJPG(filename_temp, pic, width, height);
		if (pic)
			return;
	}

	strcpy(ename, ".png");
	if (FS_CheckFile(filename_temp) != -1) {
		R_LoadPNG(filename_temp, pic, width, height);
	}
}

/**
 * @sa R_ScreenShot_f
 * @sa R_LoadJPG
 */
void R_WriteJPG (qFILE *f, byte *buffer, int width, int height, int quality)
{
	int offset, w3;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *s;

	/* Initialise the jpeg compression object */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, f->f);

	/* Setup jpeg parameters */
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	cinfo.progressive_mode = TRUE;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, qtrue);	/* start compression */
	jpeg_write_marker(&cinfo, JPEG_COM, (const byte *) GAME_TITLE, (uint32_t) strlen(GAME_TITLE));

	/* Feed scanline data */
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while (cinfo.next_scanline < cinfo.image_height) {
		s = &buffer[offset - (cinfo.next_scanline * w3)];
		jpeg_write_scanlines(&cinfo, &s, 1);
	}

	/* Finish compression */
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
}

static void R_ScaleTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	int i, j;
	unsigned frac;
	unsigned *p1, *p2;
	const unsigned fracstep = inwidth * 0x10000 / outwidth;

	p1 = (unsigned *)Mem_PoolAlloc(outwidth * outheight * sizeof(unsigned), vid_imagePool, 0);
	p2 = (unsigned *)Mem_PoolAlloc(outwidth * outheight * sizeof(unsigned), vid_imagePool, 0);

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++) {
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++) {
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	frac = fracstep >> 1;

	for (i = 0; i < outheight; i++, out += outwidth) {
		const unsigned *inrow = in + inwidth * (int) ((i + 0.25) * inheight / outheight);
		const unsigned *inrow2 = in + inwidth * (int) ((i + 0.75) * inheight / outheight);
		for (j = 0; j < outwidth; j++) {
			const byte *pix1 = (const byte *) inrow + p1[j];
			const byte *pix2 = (const byte *) inrow + p2[j];
			const byte *pix3 = (const byte *) inrow2 + p1[j];
			const byte *pix4 = (const byte *) inrow2 + p2[j];
			((byte *) (out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *) (out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *) (out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *) (out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
	Mem_Free(p1);
	Mem_Free(p2);
}

/**
 * @brief Applies brightness and contrast to the specified image while optionally computing
 * the image's average color. Also handles image inversion and monochrome. This is
 * all munged into one function to reduce loops on level load.
 */
void R_FilterTexture (unsigned *in, int width, int height, imagetype_t type)
{
	int i, j, mask;
	byte *p;
	const int c = width * height;

	switch (type) {
	case it_effect:
	case it_world:
	case it_material:
	case it_skin:
		mask = 1;
		break;
	case it_lightmap:
		mask = 2;
		break;
	default:
		mask = 0;  /* monochrome/invert */
		break;
	}

	for (i = 0, p = (byte *)in; i < c; i++, p += 4) {
		for (j = 0; j < 3; j++) {
			/* first brightness */
			float f = p[j] / 255.0;  /* as float */

			if (type != it_lightmap)  /* scale */
				f *= r_brightness->value;

			if (f > 1.0)  /* clamp */
				f = 1.0;
			else if (f < 0)
				f = 0;

			/* then contrast */
			f -= 0.5;  /* normalize to -0.5 through 0.5 */

			f *= r_contrast->value;  /* scale */

			f += 0.5;
			f *= 255;  /* back to byte */

			if (f > 255)  /* clamp */
				f = 255;
			else if (f < 0)
				f = 0;

			p[j] = (byte)f;
		}

		if (r_monochrome->integer & mask)  /* monochrome */
			p[0] = p[1] = p[2] = (p[0] + p[1] + p[2]) / 3;

		if (r_invert->integer & mask) {  /* inverted */
			p[0] = 255 - p[0];
			p[1] = 255 - p[1];
			p[2] = 255 - p[2];
		}
	}
}

/**
 * @brief Uploads the opengl texture to the server
 */
static void R_UploadTexture (unsigned *data, int width, int height, image_t* image)
{
	unsigned *scaled;
	int samples;
	int scaled_width, scaled_height;
	int i, c;
	byte *scan;
	qboolean mipmap = (image->type != it_pic && image->type != it_chars);
	qboolean clamp = (image->type == it_pic);

	for (scaled_width  = 1; scaled_width  < width;  scaled_width  <<= 1) {}
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1) {}

	while (scaled_width > r_config.maxTextureSize || scaled_height > r_config.maxTextureSize) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

	/* some images need very little attention (pics, fonts, etc..) */
	if (!mipmap && scaled_width == width && scaled_height == height) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		return;
	}

	if (scaled_width != width || scaled_height != height) {  /* whereas others need to be scaled */
		scaled = (unsigned *)Mem_PoolAlloc(scaled_width * scaled_height * sizeof(unsigned), vid_imagePool, 0);
		R_ScaleTexture(data, width, height, scaled, scaled_width, scaled_height);
	} else {
		scaled = data;
	}

	/* and filter */
	if (image->type == it_effect || image->type == it_world || image->type == it_material || image->type == it_skin)
		R_FilterTexture(scaled, scaled_width, scaled_height, image->type);
	if (image->type == it_world) {
		image->normalmap = R_FindImage(va("%s_nm", image->name), it_normalmap);
		if (image->normalmap == r_noTexture)
			image->normalmap = NULL;
	}

	/* scan the texture for any non-255 alpha */
	c = scaled_width * scaled_height;
	samples = r_config.gl_compressed_solid_format ? r_config.gl_compressed_solid_format : r_config.gl_solid_format;
	/* set scan to the first alpha byte */
	for (i = 0, scan = ((byte *) scaled) + 3; i < c; i++, scan += 4) {
		if (*scan != 255) {
			samples = r_config.gl_compressed_alpha_format ? r_config.gl_compressed_alpha_format : r_config.gl_alpha_format;
			break;
		}
	}

	image->has_alpha = (samples == r_config.gl_alpha_format || samples == r_config.gl_compressed_alpha_format);
	image->upload_width = scaled_width;	/* after power of 2 and scales */
	image->upload_height = scaled_height;

	/* and mipmapped */
	if (mipmap) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		if (r_config.anisotropic) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_config.maxAnisotropic);
			R_CheckError();
		}
		if (r_texture_lod->integer && r_config.lod_bias) {
			glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, r_texture_lod->value);
			R_CheckError();
		}
	} else {
		if (r_config.anisotropic) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
			R_CheckError();
		}
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
	}
	R_CheckError();

	if (clamp) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		R_CheckError();
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		R_CheckError();
	}

	glTexImage2D(GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	R_CheckError();

	if (scaled != data)
		Mem_Free(scaled);
}

/**
 * @brief Applies blurring to a texture
 * @sa R_BuildLightMap
 */
void R_SoftenTexture (byte *in, int width, int height, int bpp)
{
	byte *out;
	int i, j, k;
	byte *src, *dest;
	byte *u, *d, *l, *r;

	/* soften into a copy of the original image, as in-place would be incorrect */
	out = (byte *)Mem_PoolAlloc(width * height * bpp, vid_imagePool, 0);
	if (!out)
		Sys_Error("Mem_PoolAlloc: failed on allocation of %i bytes for R_SoftenTexture", width * height * bpp);

	memcpy(out, in, width * height * bpp);

	for (i = 1; i < height - 1; i++) {
		for (j = 1; j < width - 1; j++) {

			src = in + ((i * width) + j) * bpp;  /* current input pixel */

			u = (src - (width * bpp));  /* and it's neighbors */
			d = (src + (width * bpp));
			l = (src - (1 * bpp));
			r = (src + (1 * bpp));

			dest = out + ((i * width) + j) * bpp;  /* current output pixel */

			for (k = 0; k < bpp; k++)
				dest[k] = (u[k] + d[k] + l[k] + r[k]) / 4;
		}
	}

	/* copy the softened image over the input image, and free it */
	memcpy(in, out, width * height * bpp);
	Mem_Free(out);
}

#define DAN_WIDTH	512
#define DAN_HEIGHT	256

#define DAWN		0.03

/** this is the data that is used with r_dayandnightTexture */
static byte r_dayandnightAlpha[DAN_WIDTH * DAN_HEIGHT];
/** this is the mask that is used to display day/night on (2d-)geoscape */
image_t *r_dayandnightTexture;

/**
 * @brief Applies alpha values to the night overlay image for 2d geoscape
 * @param[in] q
 */
void R_CalcAndUploadDayAndNightTexture (float q)
{
	int x, y;
	const float dphi = (float) 2 * M_PI / DAN_WIDTH;
	const float da = M_PI / 2 * (HIGH_LAT - LOW_LAT) / DAN_HEIGHT;
	const float sin_q = sin(q);
	const float cos_q = cos(q);
	float sin_phi[DAN_WIDTH], cos_phi[DAN_WIDTH];
	byte *px;

	for (x = 0; x < DAN_WIDTH; x++) {
		const float phi = x * dphi - q;
		sin_phi[x] = sin(phi);
		cos_phi[x] = cos(phi);
	}

	/* calculate */
	px = r_dayandnightAlpha;
	for (y = 0; y < DAN_HEIGHT; y++) {
		const float a = sin(M_PI / 2 * HIGH_LAT - y * da);
		const float root = sqrt(1 - a * a);
		for (x = 0; x < DAN_WIDTH; x++) {
			const float pos = sin_phi[x] * root * sin_q - (a * SIN_ALPHA + cos_phi[x] * root * COS_ALPHA) * cos_q;

			if (pos >= DAWN)
				*px++ = 255;
			else if (pos <= -DAWN)
				*px++ = 0;
			else
				*px++ = (byte) (128.0 * (pos / DAWN + 1));
		}
	}

	/* upload alpha map into the r_dayandnighttexture */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, DAN_WIDTH, DAN_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, r_dayandnightAlpha);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_config.gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_config.gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	R_CheckError();
}


static const int maxAlpha = 256;		/**< Number of alpha level */
image_t *r_xviTexture;					/**< XVI texture */
static byte *r_xviPic;					/**< XVI picture */

float MAP_GetDistance(const vec2_t pos1, const vec2_t pos2);

/**
 * @brief Applies spreading on xvi transparency channel centered on a given pos
 * @param[in] pos Position of the center of XVI spreading
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 * XVI spreads in circle, and the alpha value of one pixel indicates the XVI level of infection.
 * This is necessary to take into account a new event that would spread in the zone where XVI is already spread.
 */
void R_IncreaseXVILevel (const vec2_t pos)
{
	const int bpp = 4;							/**< byte per pixel */
	const int minAlpha = 100;					/**< minimum value of alpha when spreading XVI
													255 - minAlpha is the maximum XVI level */
	int xviLevel;								/**< XVI level rate at position pos */
	vec2_t currentPos;							/**< current position (in latitude / longitude) */
	int x, y;									/**< current position (in pixel) */
	const int xviWidth = r_xviTexture->width;
	const int xviHeight = r_xviTexture->height;
	const int xCenter = bpp * round((180 - pos[0]) * r_xviTexture->width / 360.0f);
	const int yCenter = bpp * round((90 - pos[1]) * r_xviTexture->height / 180.0f);
	float radius;								/**< radius of the new XVI circle */

	assert(xCenter >= 0);
	assert(xCenter < bpp * xviWidth);
	assert(yCenter >= 0);
	assert(yCenter < bpp * xviHeight);

	if (r_xviPic[3 + yCenter * xviWidth + xCenter] < minAlpha) {
		/* This zone wasn't infected */
		r_xviPic[3 + yCenter * xviWidth + xCenter] = minAlpha;
		return;
	}

	/* Get xvi Level infection at pos */
	xviLevel = r_xviPic[3 + yCenter * xviWidth + xCenter] - minAlpha;
	/* Calculate radius of new spreading */
	xviLevel++;
	radius = sqrt(15.0f * xviLevel);

	for (y = 0; y < bpp * xviHeight; y += bpp) {
		for (x = 0; x < bpp * xviWidth; x += bpp) {
			float distance;
			Vector2Set(currentPos,
				180.0f - 360.0f * x / ((float) xviWidth * bpp),
				90.0f - 180.0f * y / ((float) xviHeight * bpp));
			distance = MAP_GetDistance(pos, currentPos);
			if (distance <= 1.1 * radius) {
				int newValue;
				if ((xCenter == x) && (yCenter == y))
					/* Make sure that XVI level increases, even if rounding problems makes radius constant */
					newValue = minAlpha + xviLevel;
				else if (distance > radius)
					newValue = round(minAlpha * (1.1 * radius - distance) / (0.1 * radius));
				else
					newValue = round(minAlpha + (xviLevel * (radius - distance)) / radius);
				if (newValue > 255) {
					Com_DPrintf(DEBUG_CLIENT, "Maximum alpha value reached\n");
					newValue = 255;
				}
				if (r_xviPic[3 + y * xviWidth + x] < newValue) {
					r_xviPic[3 + y * xviWidth + x] = newValue;
				}
			}
		}
	}

	R_BindTexture(r_xviTexture->texnum);
	R_UploadTexture((unsigned *) r_xviPic, r_xviTexture->upload_width, r_xviTexture->upload_height, r_xviTexture);
}

/**
 * @brief Initialize XVI overlay on geoscape.
 * @param[in] data Pointer to the data containing values to store in XVI map. Can be NULL for new games.
 * @param[in] width Width of data (used only if data is not NULL)
 * @param[in] height Height of data (used only if data is not NULL)
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 */
void R_InitializeXVIOverlay (const char *mapname, byte *data, int width, int height)
{
	int xviWidth, xviHeight;
	int x, y;
	byte *start;
	qboolean setToZero = qtrue;

	/* Initialize only once per game */
	if (r_xviTexture)
		return;

	/* we should always have a campaign loaded here */
	assert(mapname);

	/* Load the XVI texture */
	R_LoadImage(va("pics/geoscape/%s_xvi_overlay", mapname), &r_xviPic, &xviWidth, &xviHeight);
	if (!r_xviPic || !xviWidth || !xviHeight)
		Sys_Error("Couldn't load map mask %s_xvi_overlay in pics/geoscape", mapname);

	if (data && (width == xviWidth) && (height == xviHeight))
		setToZero = qfalse;

	/* Initialize XVI rate */
	start = r_xviPic + 3;	/* to get the first alpha value */
	for (y = 0; y < xviHeight; y++)
		for (x = 0; x < xviWidth; x++, start += 4) {
			start[0] = setToZero ? 0 : data[y * xviWidth + x];
		}

	/* Set an image */
	r_xviTexture = R_LoadImageData(va("pics/geoscape/%s_xvi_overlay", mapname), r_xviPic, xviWidth, xviHeight, it_wrappic);
}

/**
 * @brief Copy XVI transparency map for saving purpose
 * @sa CP_XVIMapSave
 */
byte* R_XVIMapCopy (int *width, int *height)
{
	int x, y;
	const int bpp = 4;
	byte *buf = Mem_PoolAlloc(r_xviTexture->height * r_xviTexture->width * bpp, vid_imagePool, 0);

	*width = r_xviTexture->width;
	*height = r_xviTexture->height;
	for (y = 0; y < r_xviTexture->height; y++) {
		for (x = 0; x < r_xviTexture->width; x++)
			buf[y * r_xviTexture->width + x] = r_xviPic[3 + (y * r_xviTexture->width + x) * bpp];
	}
	return buf;
}

/**
 * @brief Radar overlay code description
 * The radar overlay is handled in 2 times : bases radar range and aircraft radar range.
 * Bases radar range needs to be updated only when a radar facilities is built or destroyed.
 * The base radar overlay is stored in r_radarSourcePic.
 * Aircraft radar overlay needs to be updated every time an aircraft moves, because the position of the radar moves.
 * this overlay is created by duplicating r_radarSourcePic, and then adding any aircraft radar coverage. It is stored in r_radarTexture
 * @sa RADAR_UpdateWholeRadarOverlay
 */

image_t *r_radarTexture;					/**< radar texture */
static byte *r_radarPic;					/**< radar picture (base and aircraft radar) */
static byte *r_radarSourcePic;				/**< radar picture (only base radar) */

/**
 * @brief Create radar overlay on geoscape.
 * @note This function allocate memory for radar images and texture.
 * It should be called only once per game.
 */
void R_CreateRadarOverlay (void)
{
	const int radarWidth = 512;
	const int radarHeight = 256;
	const int bpp = 4;

	/* create new texture only once per life time, but reset it for every
	 * new game (campaign start or game load) */
	if (r_radarTexture) {
		memset(r_radarSourcePic, 0, bpp * radarHeight * radarWidth);
		memset(r_radarPic, 0, bpp * radarHeight * radarWidth);
		R_UploadRadarCoverage(qfalse);
		return;
	}

	r_radarPic = Mem_PoolAlloc(radarHeight * radarWidth * bpp, vid_imagePool, 0);
	r_radarSourcePic = Mem_PoolAlloc(radarHeight * radarWidth * bpp, vid_imagePool, 0);

	/* Set an image */
	r_radarTexture = R_LoadImageData("pics/geoscape/map_earth_xvi_overlay", r_radarPic, radarWidth, radarHeight, it_wrappic);
}

/**
 * @brief Initialize radar overlay on geoscape.
 * @param[in] source Initialize the source texture if qtrue: if you are updating base radar overlay.
 * false if you are updating aircraft radar overlay (base radar overlay will be copied to aircraft radar overlay)
 */
void R_InitializeRadarOverlay (qboolean source)
{
	/* Initialize Radar */
	if (source) {
		int x, y;
		const byte unexploredColor[4] = {180, 180, 180, 100}; 	/**< Color of the overlay outside radar range */

		for (y = 0; y < r_radarTexture->height; y++) {
			for (x = 0; x < r_radarTexture->width; x++) {
				memcpy(&r_radarSourcePic[4 * (y * r_radarTexture->width + x)], unexploredColor, 4);
			}
		}
	} else
		memcpy(r_radarPic, r_radarSourcePic, 4 * r_radarTexture->height * r_radarTexture->width);
}

/**
 * @brief Add a radar coverage (base or aircraft) to radar overlay
 * @param[in] pos Position of the center of radar
 * @param[in] innerRadius Radius of the radar coverage
 * @param[in] outerRadius Radius of the outer radar coverage
 * @param[in] source True if we must update the source of the radar coverage, false if the copy must be updated.
 */
void R_AddRadarCoverage (const vec2_t pos, float innerRadius, float outerRadius, qboolean source)
{
	const int bpp = 4;							/**< byte per pixel */
	const int radarWidth = r_radarTexture->width;
	const int radarHeight = r_radarTexture->height;
	vec2_t currentPos;							/**< current position (in latitude / longitude) */
	int x, y;									/**< current position (in pixel) */
	const byte innerAlpha = 0;					/**< Alpha of the inner radar range */
	const byte outerAlpha = 60;					/**< Alpha of the outer radar range */
	int yMax, yMin;					/**< Bounding box of the zone that should be drawn */

	if (pos[1] + outerRadius > 90) {
		yMax = bpp * radarHeight;
		yMin = bpp * round((90 - max(180.0f - pos[1] - outerRadius, pos[1] - outerRadius)) * radarHeight / 180.0f);
	} else if (pos[1] - outerRadius < -90) {
		yMax = bpp * round((90 - min(-180.0f - pos[1] + outerRadius, pos[1] + outerRadius)) * radarHeight / 180.0f);
		yMin = 0;
	} else {
		yMin = bpp * round((90 - pos[1] - outerRadius) * radarHeight / 180.0f);
		yMax = bpp * round((90 - pos[1] + outerRadius) * radarHeight / 180.0f);
	}

	assert(yMin >= 0);
	assert(yMin < bpp * radarHeight);

	for (y = yMin; y < yMax; y += bpp) {
		for (x = 0; x < bpp * radarWidth; x += bpp) {
			float distance;
			Vector2Set(currentPos,
				180.0f - 360.0f * x / ((float) radarWidth * bpp),
				90.0f - 180.0f * y / ((float) radarHeight * bpp));
			distance = MAP_GetDistance(pos, currentPos);
			if (distance <= outerRadius) {
				byte *dest = source ? &r_radarSourcePic[y * radarWidth + x] : &r_radarPic[y * radarWidth + x];
				if (distance > innerRadius)
					dest[3] = outerAlpha;
				else
					dest[3] = innerAlpha;
			}
		}
	}
}

/**
 * @brief Smooth radar coverage
 * @param[in] smooth Smoothes the picture if set to True (should be used only when all radars have been added)
 * @note allows to make texture pixels less visible.
 */
void R_UploadRadarCoverage (qboolean smooth)
{
	/** @todo This is no realtime function */
	if (smooth)
		R_SoftenTexture(r_radarPic, r_radarTexture->width, r_radarTexture->height, 4);

	R_BindTexture(r_radarTexture->texnum);
	R_UploadTexture((unsigned *) r_radarPic, r_radarTexture->upload_width, r_radarTexture->upload_height, r_radarTexture);
}

/**
 * @brief Creates a new image from RGBA data. Stores it in the gltextures array
 * and also uploads it.
 * @note This is also used as an entry point for the generated r_noTexture
 * @param[in] name The name of the newly created image
 * @param[in] pic The RGBA data of the image
 * @param[in] width The width of the image (power of two, please)
 * @param[in] height The height of the image (power of two, please)
 * @param[in] type The image type @sa imagetype_t
 */
image_t *R_LoadImageData (const char *name, byte * pic, int width, int height, imagetype_t type)
{
	image_t *image;
	int i;
	size_t len;

	/* find a free image_t */
	for (i = 0, image = r_images; i < r_numImages; i++, image++)
		if (!image->texnum)
			break;

	if (i == r_numImages) {
		if (r_numImages >= MAX_GL_TEXTURES)
			Com_Error(ERR_DROP, "R_LoadImageData: MAX_GL_TEXTURES hit");
		r_numImages++;
	}
	image = &r_images[i];
	image->has_alpha = qfalse;
	image->index = i;
	image->type = type;

	len = strlen(name);
	if (len >= sizeof(image->name))
		Com_Error(ERR_DROP, "R_LoadImageData: \"%s\" is too long", name);
	Q_strncpyz(image->name, name, MAX_QPATH);
	image->registration_sequence = registration_sequence;
	/* drop extension */
	if (len >= 4 && (*image).name[len - 4] == '.')
		(*image).name[len - 4] = '\0';

	image->width = width;
	image->height = height;

	if (image->type == it_pic && strstr(image->name, "_noclamp"))
		image->type = it_wrappic;

	image->texnum = TEXNUM_IMAGES + (image - r_images);
	if (pic) {
		R_BindTexture(image->texnum);
		R_UploadTexture((unsigned *) pic, width, height, image);
	}
	return image;
}

/**
 * @brief Finds or loads the given image
 * @sa R_RegisterPic
 * @param pname Image name
 * @note the image name has to be at least 5 chars long
 * @sa R_LoadTGA
 * @sa R_LoadJPG
 * @sa R_LoadPNG
 */
#ifdef DEBUG
image_t *R_FindImageDebug (const char *pname, imagetype_t type, const char *file, int line)
#else
image_t *R_FindImage (const char *pname, imagetype_t type)
#endif
{
	char lname[MAX_QPATH];
	char *ename;
	image_t *image;
	int i;
	size_t len;
	byte *pic = NULL, *palette;
	int width, height;

	if (!pname)
		Sys_Error("R_FindImage: NULL name");
	len = strlen(pname);
	if (len < 5)
		return r_noTexture;

	/* drop extension */
	Q_strncpyz(lname, pname, MAX_QPATH);
	if (lname[len - 4] == '.')
		len -= 4;
	ename = &(lname[len]);
	*ename = 0;

	/* look for it */
	for (i = 0, image = r_images; i < r_numImages; i++, image++)
		if (!Q_strncmp(lname, image->name, MAX_QPATH)) {
			image->registration_sequence = registration_sequence;
			return image;
		}

	/* load the pic from disk */
	image = NULL;
	pic = NULL;
	palette = NULL;

	strcpy(ename, ".tga");
	if (FS_CheckFile(lname) != -1) {
		R_LoadTGA(lname, &pic, &width, &height);
		if (pic) {
			image = R_LoadImageData(lname, pic, width, height, type);
			goto end;
		}
	}

	strcpy(ename, ".png");
	if (FS_CheckFile(lname) != -1) {
		R_LoadPNG(lname, &pic, &width, &height);
		if (pic) {
			image = R_LoadImageData(lname, pic, width, height, type);
			goto end;
		}
	}

	strcpy(ename, ".jpg");
	if (FS_CheckFile(lname) != -1) {
		R_LoadJPG(lname, &pic, &width, &height);
		if (pic) {
			image = R_LoadImageData(lname, pic, width, height, type);
			goto end;
		}
	}

	/* no fitting texture found */
	/* add to error list */
	image = r_noTexture;

#ifdef PARANOID
	Com_Printf("R_FindImage: Can't find %s (%s) - file: %s, line %i\n", lname, pname, file, line);
#endif

  end:
	if (pic)
		Mem_Free(pic);
	if (palette)
		Mem_Free(palette);

	return image;
}

/**
 * @brief Any image that was not touched on this registration sequence will be freed
 * @sa R_ShutdownImages
 */
void R_FreeUnusedImages (void)
{
	int i;
	image_t *image;

	R_CheckError();
	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (!image->texnum)
			continue;			/* free image slot */
		if (image->type < it_world)
			continue;			/* fix this! don't free pics */
		if (image->registration_sequence == registration_sequence)
			continue;			/* used this sequence */

		/* free it */
		glDeleteTextures(1, (GLuint *) &image->texnum);
		R_CheckError();
		memset(image, 0, sizeof(*image));
	}
}

void R_InitImages (void)
{
	int i;

	registration_sequence = 1;
	r_numImages = 0;
	r_dayandnightTexture = R_LoadImageData("***r_dayandnighttexture***", NULL, DAN_WIDTH, DAN_HEIGHT, it_effect);
	if (!r_dayandnightTexture)
		Sys_Error("Could not create daynight image for the geoscape");

	/** @todo move r_radarTexture r_xviTexture here */

	for (i = 0; i < MAX_ENVMAPTEXTURES; i++) {
		r_envmaptextures[i] = R_FindImage(va("pics/envmaps/envmap_%i.tga", i), it_effect);
		if (r_envmaptextures[i] == r_noTexture)
			Sys_Error("Could not load environment map %i", i);
	}
}

/**
 * @sa R_FreeUnusedImages
 */
void R_ShutdownImages (void)
{
	int i;
	image_t *image;

	R_CheckError();
	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (!image->texnum)
			continue;			/* free image_t slot */
		/* free it */
		glDeleteTextures(1, (GLuint *) &image->texnum);
		R_CheckError();
		memset(image, 0, sizeof(*image));
	}
}


typedef struct {
	const char *name;
	int minimize, maximize;
} glTextureMode_t;

static const glTextureMode_t gl_texture_modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};
#define NUM_R_MODES (sizeof(gl_texture_modes) / sizeof(glTextureMode_t))

void R_TextureMode (const char *string)
{
	int i, texturemode;
	image_t *image;

	for (i = 0; i < NUM_R_MODES; i++) {
		if (!Q_strcasecmp(gl_texture_modes[i].name, string))
			break;
	}

	if (i == NUM_R_MODES) {
		Com_Printf("bad filter name\n");
		return;
	}

	texturemode = i;

	for (i = 0, image = r_images; i < r_numImages; i++, image++) {
		if (image->type == it_chars || image->type == it_pic || image->type == it_wrappic)
			continue;

		R_BindTexture(image->texnum);
		if (r_config.anisotropic)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_config.maxAnisotropic);
		R_CheckError();
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_texture_modes[texturemode].minimize);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_texture_modes[texturemode].maximize);
		R_CheckError();
	}
}

typedef struct {
	const char *name;
	int mode;
} gltmode_t;

static const gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_R_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof(gltmode_t))

void R_TextureAlphaMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_ALPHA_MODES; i++) {
		if (!Q_strcasecmp(gl_alpha_modes[i].name, string))
			break;
	}

	if (i == NUM_R_ALPHA_MODES) {
		Com_Printf("bad alpha texture mode name\n");
		return;
	}

	r_config.gl_alpha_format = gl_alpha_modes[i].mode;
}

static const gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_R_SOLID_MODES (sizeof(gl_solid_modes) / sizeof(gltmode_t))

void R_TextureSolidMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_SOLID_MODES; i++) {
		if (!Q_strcasecmp(gl_solid_modes[i].name, string))
			break;
	}

	if (i == NUM_R_SOLID_MODES) {
		Com_Printf("bad solid texture mode name\n");
		return;
	}

	r_config.gl_solid_format = gl_solid_modes[i].mode;
}
