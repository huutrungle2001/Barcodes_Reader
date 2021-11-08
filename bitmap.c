#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "bitmap.h"

#define BMP_HEADER_SIZE 0x36 // Assuming windows format
#define SIZE_OFFSET 0x02
#define PIXEL_ARRAY_OFFSET 0x0A
#define WIDTH_OFFSET 0x12
#define HEIGHT_OFFSET 0x16
#define PIXEL_SIZE_OFFSET 0x1C
#define DATA_SIZE_OFFSET 0x22

#define DFRow 12
#define DFCol 8

typedef struct {
    uint32_t file_size;
    uint32_t pixel_array_offset;    
    uint32_t height;
    uint32_t width;
    uint16_t pixel_size;
    uint32_t row_size;
    uint32_t data_size;

    uint8_t *raw;
} BmpHeader;

void check_fp(FILE *fp, char *filename) {
    if(fp == NULL) {
        fprintf(stderr, "Could not open file %s\n", filename);
        exit(1);
    }
}

void assert_file_format(bool condition) {
    if (!condition) {
        fprintf(stderr, "File format error\n");
        exit(1);
    }
}

Bmp read_bmp(char *filename) {

    FILE *fp = fopen(filename, "r");
    check_fp(fp, filename);

    // Struct to return results
    Bmp bmp;
    bmp.header = malloc(sizeof(BmpHeader));
    BmpHeader *header = bmp.header;

    // Read in standard header
    uint8_t standard_header[BMP_HEADER_SIZE];
    size_t bytes_read = fread(standard_header, 1, BMP_HEADER_SIZE, fp);
    assert_file_format(bytes_read == BMP_HEADER_SIZE);

    // Check file type
    assert_file_format(standard_header[0] == 'B' && standard_header[1] == 'M');
    //printf("%x\t%x\n", standard_header, *(uint32_t *)(standard_header + SIZE_OFFSET));

    header->file_size = *((uint32_t *)(standard_header + SIZE_OFFSET));
    header->pixel_array_offset =  *((uint32_t *)(standard_header + PIXEL_ARRAY_OFFSET));

    header->pixel_size = *((uint16_t *)(standard_header + PIXEL_SIZE_OFFSET)); // Pi
    assert_file_format(header->pixel_size == 24);

    header->width =  *((uint32_t *)(standard_header + WIDTH_OFFSET));
    header->height =  *((uint32_t *)(standard_header + HEIGHT_OFFSET));

    header->row_size = ((header->pixel_size * header->width + 31) / 32) * 4;

    #ifdef DEBUG
    printf("Row size %u\n",header->row_size);
    #endif

    header->data_size = *((uint32_t *)(standard_header + 0x22));

    // Read in entire header (everything but pixel array)
    rewind(fp);
    header->raw = malloc(sizeof(unsigned char) * header->pixel_array_offset);
    assert_file_format(header->raw != NULL);
    bytes_read = fread(header->raw, 1, header->pixel_array_offset, fp);

    // Read in rest of file
    char *raw_image = malloc(header->data_size);
    bytes_read = fread(raw_image, 1, header->data_size, fp);
    assert_file_format(bytes_read == header->data_size);

    // Allocate columns
    bmp.pixels = malloc(header->height * sizeof(unsigned char **));
    for (int i = 0; i < header->height; i++) {

        // Allocate rows
        bmp.pixels[i] = malloc(header->width * sizeof(unsigned char *));
        assert_file_format(bmp.pixels[i] != NULL);

        for (int j = 0; j < header->width; j++) {

            // Allocate pixels
            bmp.pixels[i][j] = malloc(3 * sizeof(unsigned char)); 
            assert_file_format(bmp.pixels[i][j] != NULL);
        }
    }

    // Read in each pixel
    for (int y = 0; y < header->height; y++) {
        for (int x = 0; x < header->width; x++) {
            
            // Read in each pixel
            bmp.pixels[y][x][BLUE] = *((unsigned char *)(raw_image + y * header->row_size + header->pixel_size/8 * x + 0));
            bmp.pixels[y][x][GREEN] = *((unsigned char *)(raw_image + y * header->row_size + header->pixel_size/8 * x + 1));
            bmp.pixels[y][x][RED] = *((unsigned char *)(raw_image + y * header->row_size + header->pixel_size/8 * x + 2));
        }
    }

    free(raw_image);
    fclose(fp);
    assert_file_format(header->data_size + header->pixel_array_offset == header->file_size);

    // Write height and width inside output bmp wrapper
    bmp.height = header->height;
    bmp.width = header->width;


    return bmp;
}

void assert_write(bool condition) {
    if (!condition) {
        fprintf(stderr, "file write error\n");
        exit(1);
    }
}

void write_bmp(Bmp bmp, char *filename) {

    FILE *fp = fopen(filename, "w");
    check_fp(fp, filename);

    BmpHeader *header = (BmpHeader *)bmp.header;

    // Write entire header (everything but pixel array)
    size_t bytes_written = fwrite(header->raw, 1, header->pixel_array_offset, fp);
    assert_write(bytes_written == header->pixel_array_offset);

    // Write rest of file
    // Loop backward through rows (image indexed from bottom left
    for (int y = 0; y < header->height; y++) {
        for (int x = 0; x < header->width; x++) {

            unsigned char *pixel = bmp.pixels[y][x];
            fwrite(&pixel[BLUE], 1, 1, fp);
            fwrite(&pixel[GREEN], 1, 1, fp);
            fwrite(&pixel[RED], 1, 1, fp);
        }

        // Write padding
        if ((header->width * 3) % 4 != 0) {
            char null_byte = 0x00;
            for (int i = 0; i < 4 - (header->width * 3) % 4; i++) {
                fwrite(&null_byte, 1, 1, fp);
            }
        }
    }

    fclose(fp);
}

void assert_copy(bool condition) {
    if (!condition) {
        fprintf(stderr, "file write error\n");
        exit(1);
    }
}

// Copy a bmp image
Bmp copy_bmp(Bmp old_bmp) {

    BmpHeader *old_header = (BmpHeader *)old_bmp.header;

    // Copy struct
    Bmp new_bmp = old_bmp;
    new_bmp.header = NULL;
    new_bmp.pixels = NULL;

    // Copy header
    BmpHeader *header = malloc(sizeof(BmpHeader));
    assert_copy(header != NULL);
    memcpy(header, old_header, sizeof(BmpHeader));
    new_bmp.header = header;
    header->raw = NULL;

    // Copy raw header
    header->raw = malloc(sizeof(unsigned char) * old_header->pixel_array_offset);
    assert_copy(header->raw != NULL);
    memcpy(header->raw, old_header->raw, old_header->pixel_array_offset);

    // Copy rest of image
    new_bmp.pixels = calloc(header->height, sizeof(unsigned char **));
    assert_copy(new_bmp.pixels != NULL);
    for (int i = 0; i < header->height; i++) {
        new_bmp.pixels[i] = calloc(header->width, sizeof(unsigned char *));
        assert_copy(new_bmp.pixels[i] != NULL);

        for (int j = 0; j < header->width; j++) {
            new_bmp.pixels[i][j] = malloc(3 * sizeof(unsigned char)); 
            assert_copy(new_bmp.pixels[i][j] != NULL);
            memcpy(new_bmp.pixels[i][j], old_bmp.pixels[i][j], 3 * sizeof(unsigned char));
        }
    }

    return new_bmp;
}

void free_bmp(Bmp bmp) {

    BmpHeader *header = (BmpHeader *)bmp.header;

    // Free each row
    for (int i = 0; i < header->height; i++) {

        // Free each pixel
        for (int j = 0; j < header->width; j++) {
            free(bmp.pixels[i][j]); 
            bmp.pixels[i][j] = NULL;
        }
        free(bmp.pixels[i]);
        bmp.pixels[i] = NULL;
    }
    free(bmp.pixels); 
    bmp.pixels = NULL;

    // Free raw header
    if (header != NULL) {
        free(header->raw);
        header->raw = NULL;
        free(header);
    }
}

int bin_to_dec(int bin[]){
    return (int)(bin[1]*pow(2,3) + bin[2]*pow(2, 2) + bin[4]*pow(2, 1) + bin[5]*pow(2, 0))%10;
}

bool is_valid_parity(int bin[]){
    return (bin[1] + bin[2])%2 == bin[3] && (bin[4] + bin[5])%2 == bin[6];
}

bool is_reversed(Bmp bmp){
    // If the color of the fourth bit is black, the barcode is reverse
    return bmp.pixels[0][3][0] == 0;
}

// RGB(0,0,0) = black;
// 0 -> black
// black -> 1

void get_data_frame(int data_frame[][DFRow][DFCol], Bmp bmp){
    int temp_frame[bmp.height][DFRow*DFCol];
    if(!is_reversed(bmp)){
        for(int i = 0; i < bmp.height; i++){
            for(int j = 0; j < DFRow*DFCol; j++){
                temp_frame[i][j] = bmp.pixels[i][j + 3][0] == 0 ? 1 : 0;
            }
        }
    }else{
        for(int i = 0; i < bmp.height; i++){
            for(int j = 0; j < DFRow*DFCol; j++){
                temp_frame[i][DFRow*DFCol - 1 - j] = bmp.pixels[i][j + 3][0] == 0 ? 1 : 0;
            }
        }
    }

    for(int i = 0; i < bmp.height; i++){
        for(int j = 0; j < DFRow; j++) {
            for(int k = 0; k < DFCol; k++) {
                data_frame[i][j][k] = temp_frame[i][j*DFCol + k];
            }
        }
    }
}

int get_valid_row(int check_parity[][DFRow], int height){
    for(int i = 0; i < height; i++) {
        int sum = 0;
        for(int j = 0; j < DFRow; j++) {
            sum += check_parity[i][j];
        }
        // If there is no parity error, sum will be equal to DFRow
        if(sum == DFRow) {
            return i;
        }
    }

    // If there is no valid row, return -1;
    return -1;
}

void get_invalid_frame(int invalid_frame[DFRow], int check_parity[][DFRow], int height){
    for(int i = 0; i < DFRow; i++){
        int sum = 0;
        for(int j = 0; j < height; j++){
            sum += check_parity[j][i];
        }
        if(sum == 0) {
            invalid_frame[i] = 1;
        }else{
            invalid_frame[i] = 0;
        }
    }
}

int main(int argc, char** argv){
    char *filename = argv[1];
    if(filename == NULL){
        printf("No bmp image filename provided.\n");
        return 0;
    }
    char *flag = argv[argc - 1];
    Bmp bmp = read_bmp(filename);

    // Check flag
    if(strcmp(flag, "-d") == 0){
        printf("Read file %s\n", filename);
        printf("Width: %d\n", bmp.width);
        printf("Height: %d\n", bmp.height);
        return 0;
    }

    // Get DataFrame
    int data_frame[bmp.height][DFRow][DFCol];
    get_data_frame(data_frame, bmp);

    int check_parity[bmp.height][DFRow];

    for(int i = 0; i < bmp.height; i++){
        for(int j = 0; j < DFRow; j++){
            check_parity[i][j] = is_valid_parity(data_frame[i][j]);
        }
    }

    int valid_row = get_valid_row(check_parity, bmp.height);

    // If there are no valid row, show all the error columns
    if(valid_row == -1){
        int invalid_frame[DFRow];
        get_invalid_frame(invalid_frame, check_parity, bmp.height);
        int count_invalid = 0;
        int list_invalid[DFRow];
        for(int i = 0; i < DFRow; i++){
            if(invalid_frame[i] == 1){
                list_invalid[count_invalid] = i;
                count_invalid ++;
            }
        }
        if(count_invalid == 1){
            printf("Unable to read frame: %d\n", list_invalid[0]);
        }else{
            printf("Unable to read frames: ");
            for(int i = 0; i < count_invalid - 1; i++){
                printf("%d ", list_invalid[i]);
            }
            printf("%d\n", list_invalid[count_invalid - 1]);
        }

        return 0;
    }

    // If there is no parity error, show the decoded barcode
    for(int i = 0; i + 1 < DFRow; i++) 
    {
        printf("%d ", bin_to_dec(data_frame[valid_row][i]));
    }   
    printf("%d\n", bin_to_dec(data_frame[valid_row][DFRow - 1]));

}