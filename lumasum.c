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
#include <ctype.h>


const uint32_t MOD_ADLER = 65521;

uint16_t adler16(uint8_t* data, size_t size){
    uint16_t a = 1;
    for(uint32_t i=0; i < size; i++)a = (a + data[i]) % MOD_ADLER;
    return a;
}

uint8_t* rgbtoy(uint8_t* d, int x, int y){
	double pixelluma = 0;
	uint8_t* out = malloc(x*y);
	uint8_t* outcur = out;
	uint8_t* outend = out + (x*y);
	while(outcur < outend){
		pixelluma = 0.2126*d[0] + 0.7152*d[1] + 0.0722*d[2];
		outcur[0] = pixelluma;
		outcur++;
		d+=3;
	}
	return out;
}

uint8_t* getpart(uint8_t* full, int x, int h, int v, int sx, int sy){
	uint8_t* out = malloc(sx*sy);
	uint8_t* outcur = out;
	for(int py=v; py < v+sy; py++){
		for(int px=h; px < h+sx; px++){
			*outcur = full[(py*x)+px];
			outcur++;
		}
	}
	return out;
}

uint8_t lumamed(uint8_t* image, int size){
	size_t sum[256] = {0};
	size_t mediansum = 0;
	for(int i=0; i<size;i++)sum[image[i]]++;
	for(int i=0; i<256;i++){
		mediansum += sum[i];
		if(mediansum > (size/2))return i;
	}
	return 0xFF;/*We should never be there*/
}

uint8_t alphabet(uint8_t value){/*Fits (0-255) into (0-25)*/
	uint16_t m = value;
	m *= 26;
	m >>= 8;
	return m;
}

void print_lumasum(uint8_t* image, int x, int y, int type){
	uint8_t** parts = malloc(sizeof(uint8_t*) * (type*type));
	for(int yp=0; yp<type; yp++)for(int xp=0; xp<type; xp++)parts[(yp*type)+xp] = getpart(image ,x ,(x/type)*xp ,(y/type)*yp ,x/type, y/type);

	uint8_t* lumaavgs = malloc(type*type);
	for(int i=0; i<(type*type); i++){
		lumaavgs[i]=lumamed(parts[i],(x/type)*(y/type));
		printf("%c",'a' + alphabet(lumaavgs[i]) );/*Fits median luma into a-z*/
		free(parts[i]);
	}
	free(parts);
	free(lumaavgs);
}

void getsum(char* file_name, uint8_t sum_type, uint8_t iter){
	int sn, x, y;

	uint8_t* d = oioi_read(file_name, &x, &y, 3);
    if(d == NULL)d = stbi_load(file_name, &x, &y, &sn, 3);

	if(d == NULL){
		fprintf(stderr,"%s\n",stbi_failure_reason());
		return;
    }
    
    uint8_t* image = rgbtoy(d,x,y);
	free(d);

	if(iter)for(int i=1; i<=sum_type; i++)print_lumasum(image,x,y,i);
	else print_lumasum(image,x,y,sum_type);

	printf("%04x",adler16(image,x*y)); /*adler16*/
	free(image);
	printf(" %s",file_name);
	printf("\n");
}

int main(int argc, char** argv){
	if(argc < 2)return 1;
	int first_file = 1;
	uint8_t sum_type = 3;/*For 3x3 tiles, so the 9 first chars represents med. luma. Use -<number> to change.*/
	uint8_t iter = 0;
	while(argv[first_file] != NULL && argv[first_file][0] == '-'){
		if(isalpha(argv[first_file][1]) && argv[first_file][1]=='i')iter = 1;
		else if(atoi(argv[first_file]+1) > 0)sum_type = atoi(argv[first_file]+1);
		first_file++;
	}

	for(int i=first_file; argv[i]!=NULL; i++){
			getsum(argv[i], sum_type, iter);
	}
    
    return 0;
}
