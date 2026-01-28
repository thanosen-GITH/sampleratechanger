/*
 * File:   main.c
 * Author: Athanasios Karakantas
 *
 * Created on February 10, 2016, 1:32 - completed on December 11, 2022, 15:06 - improved on 28th Jan 2026, 00:24
 */

#if defined(_WIN32)
  #define fseek _fseeki64
  #define ftell _ftelli64
#endif

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <float.h>
#define max_path 1024
#define max_files 2048
#define max_data_pos 16777216 //16392 // 16384 + 8
/*
 * 
 */


typedef struct wavfile {
    char name[max_path];
    size_t size;
    unsigned int sample_rate;
    unsigned int byte_rate;
    //unsigned int mult;
    unsigned int sample_rate_pos;
    short int endianess; // 0=Little Endian, 1=Big Endian
    char suffix[5];
} wavfile;

int removeslashprefix(char *str);
int suffix(char *in,char *out);
unsigned int fmtpos(char *in,size_t readlen);
int samplerateread(char *,wavfile *,int);
int sampleratewrite(char *path,wavfile *,unsigned int,int);

static const uint8_t W64_GUID_RIFF[16] = {
    0x72,0x69,0x66,0x66, 0x2E,0x91,0xCF,0x11, 0xA5,0xD6,0x28,0xDB,0x04,0xC1,0x00,0x00
};
static const uint8_t W64_GUID_WAVE[16] = {
    0x77,0x61,0x76,0x65, 0xF3,0xAC,0xD3,0x11, 0x8C,0xD1,0x00,0xC0,0x4F,0x8E,0xDB,0x8A
};
static const uint8_t W64_GUID_FMT[16] = {
    0x66,0x6D,0x74,0x20, 0xF3,0xAC,0xD3,0x11, 0x8C,0xD1,0x00,0xC0,0x4F,0x8E,0xDB,0x8A
};
static const uint8_t W64_GUID_DATA[16] = {
    0x64,0x61,0x74,0x61, 0xF3,0xAC,0xD3,0x11, 0x8C,0xD1,0x00,0xC0,0x4F,0x8E,0xDB,0x8A
};

static int guid_eq(const uint8_t a[16], const uint8_t b[16]) {
    return memcmp(a, b, 16) == 0;
}

static uint64_t read_u64_le(const uint8_t b[8]) {
    return (uint64_t)b[0]
        | ((uint64_t)b[1] << 8)
        | ((uint64_t)b[2] << 16)
        | ((uint64_t)b[3] << 24)
        | ((uint64_t)b[4] << 32)
        | ((uint64_t)b[5] << 40)
        | ((uint64_t)b[6] << 48)
        | ((uint64_t)b[7] << 56);
}

static uint64_t align8(uint64_t x) {
    return (x + 7u) & ~7u;
}

/**
 * Finds the fmt chunk in a .w64 and returns:
 *   - *fmt_payload_pos: file offset where fmt payload begins
 *   - *fmt_payload_size: payload bytes (chunkSize - 24)
 * Returns 0 on success, -1 on failure.
 */
int w64_find_fmt(FILE *f, uint64_t *fmt_payload_pos, uint64_t *fmt_payload_size) {
    uint8_t guid[16];
    uint8_t sizeb[8];

    // Read RIFF GUID + size
    if (fread(guid, 1, 16, f) != 16) return -1;
    if (!guid_eq(guid, W64_GUID_RIFF)) return -1;
    if (fread(sizeb, 1, 8, f) != 8) return -1;
    uint64_t riff_size = read_u64_le(sizeb); // includes 24-byte header per Wave64 conventions :contentReference[oaicite:5]{index=5}
    (void)riff_size;

    // Next should be WAVE GUID
    if (fread(guid, 1, 16, f) != 16) return -1;
    if (!guid_eq(guid, W64_GUID_WAVE)) return -1;

    // Now iterate chunks until fmt found
    for (;;) {
        long long chunk_start = ftell(f);
        if (chunk_start < 0) return -1;

        if (fread(guid, 1, 16, f) != 16) return -1;
        if (fread(sizeb, 1, 8, f) != 8) return -1;

        uint64_t chunk_size = read_u64_le(sizeb);
        if (chunk_size < 24) return -1; // invalid

        uint64_t payload_size = chunk_size - 24;
        uint64_t payload_pos  = (uint64_t)chunk_start + 24;

        if (guid_eq(guid, W64_GUID_FMT)) {
            *fmt_payload_pos = payload_pos;
            *fmt_payload_size = payload_size;
            return 0;
        }

        // Skip payload + alignment padding to 8-byte boundary :contentReference[oaicite:6]{index=6}
        uint64_t next = (uint64_t)chunk_start + align8(chunk_size);
        if (fseek(f, (long long)next, SEEK_SET) != 0) return -1;
    }
}

int w64_patch_sr_and_byterate(FILE *f, uint64_t fmt_payload_pos,
                              uint32_t new_sr, uint32_t new_br)
{
    // sampleRate: fmt_payload_pos + 4
    if (fseek(f, (long long)(fmt_payload_pos + 4), SEEK_SET) != 0) return -1;
    uint8_t sr_le[4] = { (uint8_t)(new_sr),
                         (uint8_t)(new_sr >> 8),
                         (uint8_t)(new_sr >> 16),
                         (uint8_t)(new_sr >> 24) };
    if (fwrite(sr_le, 1, 4, f) != 4) return -1;

    // byteRate: fmt_payload_pos + 8
    if (fseek(f, (long long)(fmt_payload_pos + 8), SEEK_SET) != 0) return -1;
    uint8_t br_le[4] = { (uint8_t)(new_br),
                         (uint8_t)(new_br >> 8),
                         (uint8_t)(new_br >> 16),
                         (uint8_t)(new_br >> 24) };
    if (fwrite(br_le, 1, 4, f) != 4) return -1;

    return 0;
}


static double aiff_ext80_to_double(const uint8_t b[10]) {
    // b[0..1] = sign:1 + exponent:15 (bias 16383), big-endian
    // b[2..9] = mantissa (1 integer bit + 63 fraction bits), big-endian
    uint16_t se = (uint16_t)((uint16_t)b[0] << 8) | (uint16_t)b[1];
    int sign = (se & 0x8000) ? -1 : 1;
    int exp  = (int)(se & 0x7FFF);

    uint64_t mant = 0;
    for (int i = 0; i < 8; i++) {
        mant = (mant << 8) | (uint64_t)b[2 + i];
    }

    if (exp == 0 && mant == 0) return 0.0;
    if (exp == 0x7FFF) {
        // Inf/NaN – treat as invalid for sample rate use
        return NAN;
    }

    // Value = sign * mant * 2^(exp - bias - 63)
    // mant is an integer with binary point after bit 63
    const int bias = 16383;
    double m = (double)mant;
    return sign * ldexp(m, exp - bias - 63);
}

static int double_to_aiff_ext80(double x, uint8_t out[10]) {
    // Returns 0 on success, -1 if x is not representable/sane for our use.
    if (!(x > 0.0) || !isfinite(x)) return -1;

    // Clear output first
    memset(out, 0, 10);

    int sign = 0;
    if (x < 0.0) { sign = 1; x = -x; }

    // frexp: x = frac * 2^e, frac in [0.5, 1)
    int e = 0;
    double frac = frexp(x, &e);

    // Normalize to [1,2): frac *= 2, exponent--
    frac *= 2.0;
    e -= 1;

    // Mantissa is 64-bit with explicit integer bit at bit 63
    // mant = frac * 2^63
    long double m_ld = (long double)frac * (long double)(1ULL << 63);
    if (m_ld < 0.0L) return -1;
    if (m_ld > (long double)18446744073709551615.0L) {
        m_ld = (long double)18446744073709551615.0L;
    }
    uint64_t mant = (uint64_t)(m_ld + 0.5L); // round to nearest

    const int bias = 16383;
    int exp = e + bias;

    // Basic exponent sanity (for sample-rate range this won't happen)
    if (exp <= 0 || exp >= 0x7FFF) return -1;

    uint16_t se = (uint16_t)((sign ? 0x8000 : 0x0000) | (uint16_t)exp);
    out[0] = (uint8_t)(se >> 8);
    out[1] = (uint8_t)(se & 0xFF);

    // Write mantissa big-endian
    for (int i = 0; i < 8; i++) {
        out[9 - i] = (uint8_t)(mant & 0xFF);
        mant >>= 8;
    }

    return 0;
}


//========================MAIN===============================
int main(int argc, char** argv) {
    int i,j,verbose=0,filenum,fnamepos;
    
    
    char path[max_path],name[max_path], input[16]="\0";
    wavfile wav1;
    
    
    if(argc<3) {
        printf("Usage: %s -v(optional) <input files>"
            " <Correct Sample Rate>\n",argv[0]);
        return 0;
    }
    
    
    if(!strcmp(argv[1],"-v")) verbose=1;
    
    int argSR = atoi(argv[argc-1]);
    
    if((argSR<=0)||(argSR>384000)) {
        printf("Invalid value for Correct Sample Rate\n");
        return 0;
    }
    
    unsigned int SR=(unsigned int)argSR;
    
    if(verbose) {
        filenum = argc - 3;
        fnamepos = 2;
    }
    else {
        filenum = argc - 2;
        fnamepos = 1;
    }
    
    if (filenum>max_files) {
        printf("Maximum number of files is %d!\n",max_files);
        return -1;
    }
    printf("\n=============================================================\n");
    printf("Sample Rate will be changed and"
                " the original file(s) overwritten!\n");
    printf("\tAre you sure? (y/n)\n");
    printf("=============================================================\n");
    printf("->");
    fgets(input,sizeof(input),stdin);
     while((strncasecmp(input,"y\n",2)) && (strncasecmp(input,"n\n",2))) {
            printf("<y/n>\n>");
            fgets(input,sizeof(input),stdin);
        }
    if(strcasecmp(input,"y\n")) {
        printf("Canceling... Bye!\n");
        return 0;
    }
    
    for(j=0;j<filenum;j++) {
        
        strcpy(path,argv[fnamepos]);
        strcpy(name,argv[fnamepos]);
        i = removeslashprefix(name);
        if(i<0) {
            printf("Problem with removing path prefix from %s\n",path);
            return -1;
        }
        printf("\n======PROCESSING %s======\n",name);
        i = samplerateread(path,&wav1,verbose);
    
        if(i<0) {
            printf("Problem with Sampleread of %s!\n",name);
            if(j<filenum-1) printf("Skipping to next file...\n");
            fnamepos++;
            continue;
        }

        if(wav1.sample_rate!=SR) {

            //SR = SR/wav1.mult;
            i = sampleratewrite(path,&wav1,SR,verbose);
            if(i<0) {
                printf("Processing of %s failed due to data input error!, Exiting\n",
                    name);
                return -1;
            }
            else printf("======DONE======\n");


        }

        else printf("======(Sample rate of %s is correct)======\n",name);
        
        fnamepos++;
    }
    
    
    printf("\nBye\n");
    //printf("FOR AIFF: http://aeroquartet.com/movierepair/aiff\n"
      //      "http://www.onicos.com/staff/iz/formats/aiff.html\n");
    
    
    return (EXIT_SUCCESS);
}

//========================MAIN===============================

int removeslashprefix(char *str) {
    int i, j, slashindex=0, found=0;
    if(!str) {
        printf("removeslashprefix: string is null\n");
        return -1;
    }
    //printf("%s to have slash removed\n",str);
    i=0;
    while(str[i] != '\0' && i<max_path) { // FIND LAST / e.g. 3/qwe.c
        if(str[i] == '/') {
            slashindex = i; // 1
            found = 1; // we found one
        }
        i++;
    }
    if(i==max_path && !found) return -1;
    //printf("in here found = %d, slash on %d of %d\n",found,slashindex,i);

    if (found==1) {
        for(j=0;j<i-slashindex-1;j++) { // 0-4
        str[j] = str[slashindex+j+1]; // i0 = i2
        }
    str[i-slashindex-1] = '\0';
    }
    //printf("%s\n",str);

    return found;

}


int suffix(char *in,char *out) {
    int l = strlen(in);
    if(l<=0) {
        printf("Invalid filename input for suffix\n");
        return -1;
    }
    
    
    int i = l - 1;
    while (i >= 0 && in[i] != '.') i--;
    if (i < 0 || i == l - 1) return -1;   // no dot or dot is last char

    int j=0;
    i++;
    while(i<l) {
        out[j] = in[i];
        i++;
        j++;
    }
    out[j] = '\0';
    
    return 0;
}

unsigned int fmtpos(char *in,size_t len) {
    if(in==NULL) {
        printf("Null buffer, fmt position = 0\n");
        return 0;
    }
    else if (len<4) {
        printf("Buffer too small, fmt position = 0\n");
        return 0;
    }
    
    else;
    
    int i=0;
    unsigned int pos=0;
    while(memcmp(in+i,"fmt ",4) && i<=len-4) {
        i++;
        if(i==len-3) {
                printf("Reached max search length and didn't find ""fmt""\n");
                return 0;
            }
    }
    pos = i;
    
    return pos;
}

int samplerateread(char *path,wavfile *w,int v) {
    if(path==NULL) {
        printf("Null file Pointer! Sample read failed!\n");
        return -1;
    }
    else if(w==NULL) {
        printf("Null wavfile struct! Sample read failed!\n");
        return -1;
    }
    else ;
    
    int i;
    char internal_buffer[9]="\0",*header_buffer=NULL;
    
    header_buffer = malloc(max_data_pos); if(!header_buffer) {
        perror("samplerateread: error allocating memory for buffer:");
        return -1;
    }

    FILE *f=fopen(path,"rb");
    if(f==NULL) {
        if(header_buffer) { free(header_buffer); header_buffer = NULL; }
        perror(path);
        return -1;
    }
    
    strcpy(w->name,path);
    i = removeslashprefix(w->name);
    
    if(v) printf("%s opened\n",w->name);
    suffix(w->name,w->suffix);

    if(fseek(f,0,SEEK_END)) { // go to end
        perror("samplerateread: fseek:");
        if(header_buffer) { free(header_buffer); header_buffer = NULL; }
        return -1;
    }
    w->size = ftell(f); // store as size the end position indicator

    if(v) printf("Size of %s is %lu\n",w->name,w->size);
    
    if(fseek(f,0,SEEK_SET)) { // go back
        perror("samplerateread: fseek:");
        if(header_buffer) { free(header_buffer); header_buffer = NULL; }
        return -1;
    }
    size_t readlen = max_data_pos;
    if(readlen>w->size) readlen = w->size; 

    if(!strcasecmp(w->suffix,"aif")||!strcasecmp(w->suffix,"aiff")) {
        
        i=fread(header_buffer,1,readlen,f);
    
        if(i!=readlen) {
            perror("read file problem");
            if(header_buffer) { free(header_buffer); header_buffer = NULL; }
            fclose(f);
            return -1;
        }
        
        i=0;
        
        while (strncmp("COMM",header_buffer+i,4)) {
            i++;
            if(i==readlen-3) {
                if(header_buffer) { free(header_buffer); header_buffer = NULL; }
                printf("Reached max search length and didn't find ""COMM""\n");
                return -1;
            }
        }
        
        if(v) {
            printf("Found COMM!\nByte %d\n",i);
            printf("Sample Rate Byte should be %d\n",i+16);
        }
        
        w->sample_rate_pos=i+16;

        if ((size_t)w->sample_rate_pos + 10 > readlen) {
            printf("AIFF: COMM sampleRate out of read buffer bounds.\n");
            free(header_buffer);
            fclose(f);
            return -1;
        }

        double sr_d = aiff_ext80_to_double((uint8_t*)header_buffer + w->sample_rate_pos);
        if (!isfinite(sr_d) || sr_d <= 0.0) {
            printf("AIFF: invalid sampleRate ext80.\n");
            free(header_buffer);
            fclose(f);
            return -1;
        }

        // Store as integer Hz (rounded)
        w->sample_rate = (unsigned int)llround(sr_d);

        if (v) printf("AIFF sampleRate = %.6f (stored %u)\n", sr_d, w->sample_rate);

        if(v) printf("Big Endian\n");
        w->endianess=1; // AIFF always big endian
        
        w->byte_rate=0; // AIFF has no byte rate field stored in file

        free(header_buffer);
        fclose(f);
        return 0;
    }

    if(!strcasecmp(w->suffix,"w64")) {
        w->endianess = 0;
        uint64_t fmt_payload_pos = 0, fmt_payload_size = 0;
        if(w64_find_fmt(f,&fmt_payload_pos,&fmt_payload_size)) { fclose(f); if(header_buffer) { free(header_buffer); header_buffer = NULL; } return -1;}
        if(fseek(f,(long long)(fmt_payload_pos+4),SEEK_SET)) {
            perror("samplerateread: fseek");
            if(header_buffer) { free(header_buffer); header_buffer = NULL; }
            fclose(f);
            return -1;
        }

        i=fread(header_buffer,1,16,f);
    
        if(i!=16) {
            perror("read file problem");
            if(header_buffer) { free(header_buffer); header_buffer = NULL; }
            fclose(f);
            return -1;
        }

        w->sample_rate_pos = fmt_payload_pos + 4;
        w->sample_rate=(int)(unsigned char)header_buffer[0] + 
        (int)(unsigned char)header_buffer[1]*256 + 
        (int)(unsigned char)header_buffer[2]*65536 +
        (int)(unsigned char)header_buffer[3]*16777216;

        w->byte_rate = (int)(unsigned char)header_buffer[4] +
        (int)(unsigned char)header_buffer[5]*256 +
        (int)(unsigned char)header_buffer[6]*65536 +
        (int)(unsigned char)header_buffer[7]*16777216;

        if(header_buffer) { free(header_buffer); header_buffer = NULL; }
        fclose(f);
        return 0;

    }
    
    i=fread(header_buffer,1,readlen,f);
    
    if(i!=readlen) {
        perror("read file problem");
        if(header_buffer) { free(header_buffer); header_buffer = NULL; }
        fclose(f);
        return -1;
    }
    
    strncpy(internal_buffer,header_buffer,4);
    
    if(!strncmp(internal_buffer,"RIFX",4)|| !strncmp(internal_buffer,"FORM",4)) {
        if(v) printf("Big Endian\n");
        w->endianess=1;
    }
    
    else {
        if(v) printf("Little Endian\n");
        w->endianess=0;
    }
    
    if(!strncmp(internal_buffer,"FFIR",4)) {
        printf("Warning: byte-swapped RIFF detected (FFIR). File may be corrupted.\n");
        if(header_buffer) { free(header_buffer); header_buffer = NULL; }
        return -1;
    }
    
    w->sample_rate_pos = fmtpos(header_buffer,readlen)+12;
    
    if(w->sample_rate_pos<13) {
        if(header_buffer) { free(header_buffer); header_buffer = NULL; }
        printf("Sample rate position byte too low! Exiting\n");
        return -1;
    }
    printf("Found fmt!\nByte %u\nSample Rate Byte should be %u\n",
            w->sample_rate_pos-12,w->sample_rate_pos);
    
    if(w->endianess) {
        w->sample_rate=(int)(unsigned char)header_buffer[w->sample_rate_pos+3] + 
        (int)(unsigned char)header_buffer[w->sample_rate_pos+2]*256 +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+1]*65536 +
        (int)(unsigned char)header_buffer[w->sample_rate_pos]*16777216;
        w->byte_rate = (int)(unsigned char)header_buffer[w->sample_rate_pos+7] +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+6]*256 +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+5]*65536 +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+4]*16777216;
    }
    else {
        w->sample_rate=(int)(unsigned char)header_buffer[w->sample_rate_pos] + 
        (int)(unsigned char)header_buffer[w->sample_rate_pos+1]*256 + 
        (int)(unsigned char)header_buffer[w->sample_rate_pos+2]*65536 +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+3]*16777216;
        w->byte_rate = (int)(unsigned char)header_buffer[w->sample_rate_pos+4] +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+5]*256 +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+6]*65536 +
        (int)(unsigned char)header_buffer[w->sample_rate_pos+7]*16777216;
    }
    
    if(v) printf("Current SR %d\n",w->sample_rate);

    if(header_buffer) { free(header_buffer); header_buffer = NULL; }
    
    fclose(f);
    
    return 0;
    
}

int sampleratewrite(char *path,wavfile *w,unsigned int SR,int v) {
    if (w==NULL) {
        printf("No wav structure available for Sample Rate write!\n");
        return -1;
    }
    else if((SR==0)||(SR>384000)) {
        printf("Invalid Sample Rate for Sample Rate Write!\n");
        return -1;
    }
    else if(w->sample_rate==SR) {
        printf("Correct sample rate, no need to change\n");
        return 0;
    }
    else ;
    
    FILE *f=fopen(path,"r+b");
    if(f==NULL) {
        perror(w->name);
        return 0;
    }
        
    printf("%s <- changing sample rate...\n",w->name);

    int i=fseek(f,w->sample_rate_pos,SEEK_SET);
    if(i!=0) {
        perror("sampleratewrite: fseek:");
        fclose(f);
        return 0;
    }
    //printf("1\n"); getchar(); //Debugging

    if(!strcasecmp(w->suffix,"aif")||!strcasecmp(w->suffix,"aiff")) {
        // w->sample_rate_pos should point to the start of the 10-byte ext80 sampleRate in COMM
        if (fseek(f, w->sample_rate_pos, SEEK_SET) != 0) {
            perror("sampleratewrite(AIFF): fseek");
            fclose(f);
            return 0;
        }

        uint8_t ext[10];
        if (double_to_aiff_ext80((double)SR, ext) != 0) {
            printf("AIFF: cannot encode SR=%u into ext80.\n", SR);
            fclose(f);
            return 0;
        }

        if (fwrite(ext, 1, 10, f) != 10) {
            perror("sampleratewrite(AIFF): fwrite");
            fclose(f);
            return 0;
        }

        fclose(f);
        return 0;
    }


    if (!strcasecmp(w->suffix, "w64")) {
    // W64 always little-endian
        uint64_t fmt_payload_pos = 0, fmt_payload_size = 0;

        if (fseek(f, 0, SEEK_SET) != 0) { perror("w64: fseek"); fclose(f); return 0; }
        if (w64_find_fmt(f, &fmt_payload_pos, &fmt_payload_size) != 0) {
            perror("w64: find fmt");
            fclose(f);
            return 0;
        }

        // Scale byteRate using old header values (your current approach)
        uint32_t new_br = 0;
        if (w->sample_rate != 0 && w->byte_rate != 0) {
            uint64_t tmp = (uint64_t)w->byte_rate * (uint64_t)SR;
            new_br = (uint32_t)(tmp / (uint64_t)w->sample_rate);
        }

        if (w64_patch_sr_and_byterate(f, fmt_payload_pos, (uint32_t)SR, new_br) != 0) {
            perror("w64: patch sr/byterate");
            fclose(f);
            return 0;
        }

        fclose(f);
        return 0;
    }

    unsigned char byte[4] = {(SR & 0xFF000000) >> 24, (SR & 0x00FF0000) >> 16,
       (SR & 0x0000FF00) >> 8, SR & 0x000000FF};

    uint32_t byteRate = 0; bool byteRate_ok = false;
    if (w->sample_rate != 0 && w->byte_rate != 0) {
        uint64_t tmp = (uint64_t)w->byte_rate * (uint64_t)SR;
        byteRate = (uint32_t)(tmp / (uint64_t)w->sample_rate);
        byteRate_ok = true;
    }

    if(w->endianess) {
        if(fwrite(byte,1,4,f)!=4) { perror("sampleratewrite: fwrite"); return 0; }
    }
    else {
        unsigned char byte_l_endian[4] = {byte[3],byte[2],byte[1],byte[0]};
        if(fwrite(byte_l_endian,1,4,f)!=4) { perror("sampleratewrite: fwrite"); fclose(f); return 0; }
    }

    if(byteRate_ok) {
        unsigned char ratebyte[4] = {(byteRate >> 24) & 0xFF, (byteRate >> 16) & 0xFF,
                (byteRate >> 8)  & 0xFF, byteRate        & 0xFF};
        if(w->endianess) {
            if (fwrite(ratebyte,1,4,f)!=4) { perror("sampleratewrite: fwrite"); return 0; }
        }
        else {
            unsigned char ratebyte_l_endian[4] = {ratebyte[3],ratebyte[2],ratebyte[1],ratebyte[0]};
            if(fwrite(ratebyte_l_endian,1,4,f)!=4) { perror("sampleratewrite: fwrite"); fclose(f); return 0; }
        }
    }

    fclose(f);
    
    return 0;
}
