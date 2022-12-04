#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#define STBI_FAILURE_USERMSG
#define STBI_NO_HDR
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "stb_image.h"
#define OIOI_IMPLEMENTATION
#include "oioi.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>


const uint32_t MOD_ADLER = 65521;

uint16_t adler16(unsigned char *data, size_t size){
    uint16_t a = 1;
    for (uint32_t i=0; i < size; i++)a = (a + data[i]) % MOD_ADLER;
    return a;
}

unsigned char* rgbtoy(unsigned char* d, int x, int y){
	double pixelluma = 0;
	unsigned char* out = malloc(x*y);
	unsigned char* outcur = out;
	unsigned char* outend = out + (x*y);
	while(outcur < outend){
		pixelluma = 0.2126*d[0] + 0.7152*d[1] + 0.0722*d[2];
		outcur[0] = pixelluma;
		outcur++;
		d+=3;
	}
	return out;
}

unsigned char* getpart(unsigned char* full, int x, int h, int v, int sx, int sy){
	unsigned char* out = malloc(sx*sy);
	unsigned char* outcur = out;
	for(int py=v; py < v+sy; py++){
		for(int px=h; px < h+sx; px++){
			*outcur = full[(py*x)+px];
			outcur++;
		}
	}
	return out;
}

unsigned char lumamed(unsigned char* image, int size){
	unsigned long sum[256] = {0};
	unsigned long mediansum = 0;
	for(int i=0; i<size;i++)sum[image[i]]++;
	for(int i=0; i<256;i++){
		mediansum += sum[i];
		if(mediansum > (size/2))return i;
	}
	return 0xFF;//We should never be there
}

unsigned char alphabet(unsigned char value){//Fits (0-255) into ('a'-'z')
	unsigned int m = value;
	m *= 26;
	m /= 256;
	return m;
}

unsigned char* lumasum(unsigned char* d, int x, int y, int type){
	unsigned char* image = rgbtoy(d,x,y);
	unsigned char** parts = malloc(sizeof(unsigned char*) * (type*type));
	for(int yp=0; yp<type; yp++)for(int xp=0; xp<type; xp++)parts[(yp*type)+xp] = getpart(image ,x ,(x/type)*xp ,(y/type)*yp ,x/type, y/type);
	
	unsigned char* lumaavgs = malloc(type*type);
	for(int i=0; i<(type*type); i++){
		lumaavgs[i]=lumamed(parts[i],(x/type)*(y/type));
		printf("%c",'a' + alphabet(lumaavgs[i]) );//Fits median luma into alphabet
		free(parts[i]);
	}
	free(parts);
	free(lumaavgs);
	return image;
}

void getsum(char* img, char sum_type){
	int sn;
	int x,y;
	unsigned char* d = oioi_read(img, &x, &y, 3);
    if(d == NULL)d = stbi_load(img, &x, &y, &sn, 3);

	if(d == NULL){
		fprintf(stderr,"%s\n",stbi_failure_reason());
		return;
    }
    
	unsigned char* image = lumasum(d,x,y,sum_type);
	free(d);
	
	printf("%04x",adler16(image,x*y));                    //adler16
	free(image);
	printf(" %s",img);                                   //File name
	printf("\n");
}

int main(int argc, char** argv){
	if(argc < 2)return 1;
	int first_file = 1;
	int sum_type = 3;//For 3x3 tiles, so the 9 first chars represents med. luma. Use -number to change.
	if(argv[1][0] == '-' && atoi(argv[1]+1) > 0){
		sum_type = atoi(argv[1]+1);
		first_file++;
	}

	for(int i=first_file; argv[i]!=NULL; i++){
			getsum(argv[i], sum_type);
	}
    
    return 0;
}
