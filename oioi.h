//oioi (おいおい) - Single-file public-domain QOI file reader.
#ifndef OIOI_H
#define OIOI_H
#ifdef __cplusplus
extern "C" {
#endif

//Returns a decoded image of chan bpp, size *w x *h, from file. Or NULL if it fails.
unsigned char* oioi_read(char* file, int* w, int* h, char chan);

//Returns a decoded image of chan bpp, size *w x *h, from memory. Or NULL if it fails.
unsigned char* oioi_decode(void* mem, unsigned long size, int* w, int* h, char chan);

#ifdef __cplusplus
}
#endif
#endif //OIOI_H

#ifdef OIOI_IMPLEMENTATION

#include <stdio.h>//fread, fopen, fclose
#include <stdlib.h>//malloc, free
#include <string.h>//memcpy, memcmp

#define OIOI_BUF_FILE 512
struct oioi_file{
	FILE* fp;
	unsigned char* buf;
	unsigned int pos;//position in the buffer
	unsigned int esize;//effective size of the buffer;
};
typedef struct oioi_file oioi_file;

oioi_file* oioi_file_open(char* file, char* access){
	oioi_file* f = malloc(sizeof(oioi_file));
	f->fp = fopen(file,access);
	f->buf = malloc(OIOI_BUF_FILE);
	f->pos = 0;
	f->esize = 0;
	return f;
}

oioi_file* oioi_file_memory(void* mem, unsigned long size){
	oioi_file* f = malloc(sizeof(oioi_file));
	f->fp = NULL;
	f->buf = mem;
	f->pos = 0;
	f->esize = size;
	return f;
}

void oioi_file_read(unsigned char* ptr, char eltsize, unsigned int n, oioi_file* f){
	if((f->pos + n) < f->esize){
		memcpy(ptr,f->buf + f->pos,n);
		f->pos += n;
		return;
	}
	do{
		if(f->pos == f->esize && f->fp != NULL){
			f->esize = fread(f->buf, 1, OIOI_BUF_FILE, f->fp);
			f->pos = 0;
		}
		*(ptr++) = f->buf[f->pos++];
		n--;
	}while(n > 0);
}

void oioi_file_close(oioi_file* f){
	if(f->fp != NULL){free(f->buf);fclose(f->fp);}
	free(f);
}

#define oioi_index(R,G,B,A) (R * 3 + G * 5 + B * 7 + A * 11) % 64

unsigned char* oioi_dec(void* img, int* w, int* h, char chan){
	//check first if file is a qoi file:
	unsigned char magic[4];
	oioi_file_read(magic,1,4,img);
	if(memcmp(magic,"qoif",4))return NULL;//Invalid file type
	
	//we assume starting here that the file is valid
	unsigned char width[4]; oioi_file_read(width,1,4,img);
	unsigned char height[4]; oioi_file_read(height,1,4,img);
	unsigned char actual_chan; oioi_file_read(&actual_chan,1,1,img);
	unsigned char colorspace; oioi_file_read(&colorspace,1,1,img);
	*w = width[3] | (width[2] << 8) | (width[1] << 16) | (width[0] << 24);
	*h = height[3] | (height[2] << 8) | (height[1] << 16) | (height[0] << 24);
	
	//do we need to do further conversion after ?
	char do_conversion = 0;
	if(chan == 3 && actual_chan == 4){chan = actual_chan; do_conversion = 1;}
	
	//allocate buffer
	const unsigned long pixels = *w * *h;
	unsigned char* out = malloc(pixels * chan);
	if(out == NULL)return NULL;
	
	//decode here
	unsigned char index[64*4] = {0};
	unsigned char prev[4] = {0,0,0,255};
	unsigned char chunk[5] = {0};
	unsigned int i = 0;
	while(i < pixels){
		oioi_file_read(chunk,1,1,img);
		switch(chunk[0]){//"A decoder must check for the presence of an 8-bit tag first."
			case 254://QOI_OP_RGB
				oioi_file_read(chunk+1,1,3,img);
				memcpy(prev, chunk+1, 3);
				memcpy(out + (i*chan), prev, chan);
				break;
			case 255://QOI_OP_RGBA
				oioi_file_read(chunk+1,1,4,img);
				memcpy(prev, chunk+1, 4);
				memcpy(out + (i*chan), prev, chan);
				break;
			default:
				switch(chunk[0] & 192){
					case 192://QOI_OP_RUN
						for(unsigned char repeat = 0; repeat < (chunk[0] & 63)+1; repeat++){
							memcpy(out + (i*chan), prev, chan);
							i++;
						}
						i--;
						break;
					case 128://QOI_OP_LUMA
						oioi_file_read(chunk+1,1,1,img);
						prev[1] = prev[1] - 32 + (chunk[0] & 63);//G
						prev[0] = prev[0] - 8 + ((chunk[1] & 240)>>4) - 32 + (chunk[0] & 63);//R
						prev[2] = prev[2] - 8 + (chunk[1] & 15) - 32 + (chunk[0] & 63);//B
						memcpy(out + (i*chan), prev, chan);
						break;
					case 64://QOI_OP_DIFF
						prev[0] = prev[0] - 2 + ((chunk[0] & 48)>>4);
						prev[1] = prev[1] - 2 + ((chunk[0] & 12)>>2);
						prev[2] = prev[2] - 2 + (chunk[0] & 3);
						memcpy(out + (i*chan), prev, chan);
						break;
					case 0://QOI_OP_INDEX
						memcpy(prev, index + (chunk[0]<<2), 4);
						memcpy(out + (i*chan), prev, chan);
						break;
					default:
						return NULL;
				}
		}
		memcpy(index + (oioi_index(prev[0],prev[1],prev[2],prev[3])<<2), prev, 4);
		i++;
	}
	if(do_conversion){//convert alpha if user asked for RGB
		unsigned char* conv = malloc(pixels * 3);
		for(i = 0; i < pixels; i++){
			conv[(i*3)] = (out[(i*4)] * out[(i*4)+3])/255;
			conv[(i*3)+1] = (out[(i*4)+1] * out[(i*4)+3])/255;
			conv[(i*3)+2] = (out[(i*4)+2] * out[(i*4)+3])/255;
		}
		free(out);
		out = conv;
	}
	return out;
	
}

unsigned char* oioi_decode(void* mem, unsigned long size, int* w, int* h, char chan){
	if(chan != 3 && chan != 4)return NULL;//Invalid chan
	oioi_file* img;
	
	img = oioi_file_memory(mem, size);
	unsigned char* r = oioi_dec(img, w, h, chan);
	oioi_file_close(img);
	
	return r;
}

unsigned char* oioi_read(char* file, int* w, int* h, char chan){
	if(chan != 3 && chan != 4)return NULL;//Invalid chan
	oioi_file* img;
	
	//check if file is a valid file:
	img = oioi_file_open(file,"rb");
	if(img == NULL)return NULL;//Invalid file
	
	unsigned char* r = oioi_dec(img, w, h, chan);
	
	oioi_file_close(img);
	
	return r;
}

#endif //OIOI_IMPLEMENTATION

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2022 evv42
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
