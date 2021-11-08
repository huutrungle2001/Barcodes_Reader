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

#define DFROW 12
#define DFCOL 8
#define BARCODECOLUMS 102

typedef struct
{
    uint32_t file_size;
    uint32_t pixel_array_offset;
    uint32_t height;
    uint32_t width;
    uint16_t pixel_size;
    uint32_t row_size;
    uint32_t data_size;

    uint8_t *raw;
} BmpHeader;

typedef struct
{
    int bin[DFCOL];
    const int nbBits = DFCOL;

} BarCodeFrame;
typedef struct
{
    BarCodeFrame barCodeFrames[DFROW];
    const int nbFrames = DFROW;
} BarCodeRow;

#define FRAMES 12
typedef struct
{
    BarCodeRow barCodeRows[10000];
    int nbRows;
    int barCodeValues[FRAMES];
    int nbErrorFrames = 0;
    int errorFrameIndices[FRAMES];
    const int NBFRAMES = FRAMES;

} BarCode;

void check_fp(FILE *fp, char *filename)
{
    if (fp == NULL)
    {
        fprintf(stderr, "Could not open file %s\n", filename);
        exit(1);
    }
}

void assert_file_format(bool condition)
{
    if (!condition)
    {
        fprintf(stderr, "File format error\n");
        exit(1);
    }
}

Bmp read_bmp(char *filename)
{

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
    // printf("%x\t%x\n", standard_header, *(uint32_t *)(standard_header + SIZE_OFFSET));

    header->file_size = *((uint32_t *)(standard_header + SIZE_OFFSET));
    header->pixel_array_offset = *((uint32_t *)(standard_header + PIXEL_ARRAY_OFFSET));

    header->pixel_size = *((uint16_t *)(standard_header + PIXEL_SIZE_OFFSET)); // Pi
    assert_file_format(header->pixel_size == 24);

    header->width = *((uint32_t *)(standard_header + WIDTH_OFFSET));
    header->height = *((uint32_t *)(standard_header + HEIGHT_OFFSET));

    header->row_size = ((header->pixel_size * header->width + 31) / 32) * 4;

#ifdef DEBUG
    printf("Row size %u\n", header->row_size);
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
    for (int i = 0; i < header->height; i++)
    {

        // Allocate rows
        bmp.pixels[i] = malloc(header->width * sizeof(unsigned char *));
        assert_file_format(bmp.pixels[i] != NULL);

        for (int j = 0; j < header->width; j++)
        {

            // Allocate pixels
            bmp.pixels[i][j] = malloc(3 * sizeof(unsigned char));
            assert_file_format(bmp.pixels[i][j] != NULL);
        }
    }

    // Read in each pixel
    for (int y = 0; y < header->height; y++)
    {
        for (int x = 0; x < header->width; x++)
        {

            // Read in each pixel
            bmp.pixels[y][x][BLUE] = *((unsigned char *)(raw_image + y * header->row_size + header->pixel_size / 8 * x + 0));
            bmp.pixels[y][x][GREEN] = *((unsigned char *)(raw_image + y * header->row_size + header->pixel_size / 8 * x + 1));
            bmp.pixels[y][x][RED] = *((unsigned char *)(raw_image + y * header->row_size + header->pixel_size / 8 * x + 2));
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

void assert_write(bool condition)
{
    if (!condition)
    {
        fprintf(stderr, "file write error\n");
        exit(1);
    }
}

void write_bmp(Bmp bmp, char *filename)
{

    FILE *fp = fopen(filename, "w");
    check_fp(fp, filename);

    BmpHeader *header = (BmpHeader *)bmp.header;

    // Write entire header (everything but pixel array)
    size_t bytes_written = fwrite(header->raw, 1, header->pixel_array_offset, fp);
    assert_write(bytes_written == header->pixel_array_offset);

    // Write rest of file
    // Loop backward through rows (image indexed from bottom left
    for (int y = 0; y < header->height; y++)
    {
        for (int x = 0; x < header->width; x++)
        {

            unsigned char *pixel = bmp.pixels[y][x];
            fwrite(&pixel[BLUE], 1, 1, fp);
            fwrite(&pixel[GREEN], 1, 1, fp);
            fwrite(&pixel[RED], 1, 1, fp);
        }

        // Write padding
        if ((header->width * 3) % 4 != 0)
        {
            char null_byte = 0x00;
            for (int i = 0; i < 4 - (header->width * 3) % 4; i++)
            {
                fwrite(&null_byte, 1, 1, fp);
            }
        }
    }

    fclose(fp);
}

void assert_copy(bool condition)
{
    if (!condition)
    {
        fprintf(stderr, "file write error\n");
        exit(1);
    }
}

// Copy a bmp image
Bmp copy_bmp(Bmp old_bmp)
{

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
    for (int i = 0; i < header->height; i++)
    {
        new_bmp.pixels[i] = calloc(header->width, sizeof(unsigned char *));
        assert_copy(new_bmp.pixels[i] != NULL);

        for (int j = 0; j < header->width; j++)
        {
            new_bmp.pixels[i][j] = malloc(3 * sizeof(unsigned char));
            assert_copy(new_bmp.pixels[i][j] != NULL);
            memcpy(new_bmp.pixels[i][j], old_bmp.pixels[i][j], 3 * sizeof(unsigned char));
        }
    }

    return new_bmp;
}

void free_bmp(Bmp bmp)
{

    BmpHeader *header = (BmpHeader *)bmp.header;

    // Free each row
    for (int i = 0; i < header->height; i++)
    {

        // Free each pixel
        for (int j = 0; j < header->width; j++)
        {
            free(bmp.pixels[i][j]);
            bmp.pixels[i][j] = NULL;
        }
        free(bmp.pixels[i]);
        bmp.pixels[i] = NULL;
    }
    free(bmp.pixels);
    bmp.pixels = NULL;

    // Free raw header
    if (header != NULL)
    {
        free(header->raw);
        header->raw = NULL;
        free(header);
    }
}

bool parity_check(int bin[])
{
    return ((bin[1] + bin[2]) % 2 == bin[3] && (bin[4] + bin[5]) % 2 == bin[6]);
}
int bin_to_dec(int bin[])
{
    bool check_parity = parity_check(bin);

    if (check_parity)
        return (int)(bin[1] * pow(2, 3) + bin[2] * pow(2, 2) + bin[4] * pow(2, 1) + bin[5] * pow(2, 0)) % 10;
    return -1;
}

int is_reversed(Bmp bmp)
{
    return bmp.pixels[0][3][0] == 0;
}

int is_reversed_image(Bmp *bmp)
{
    return bmp->pixels[0][3][0] == 0;
}

#define START_POS_OF_EACH_ROW 3
#define END_POS_OF_EACH_ROW 100
#define BLACK 1
#define WHITE 0
#define ZERO 0

int getBit(Bmp *bmp, int row, int col)
{
    return (bmp->pixels[row][col][0] == ZERO) ? BLACK : WHITE;
}

void get_data_frame(BarCode *barCode, Bmp *bmp)
{
    barCode->nbRows = bmp->height;

    for (int row = 0, col = START_POS_OF_EACH_ROW; row < bmp->height; row++)
    {
        for (int frameIndex = 0; frameIndex < DFROW; frameIndex++)
        {
            for (int bitIndex = 0; bitIndex < DFCOL; bitIndex++)
            {
                barCode->barCodeRows[row].barCodeFrames[frameIndex].bin[bitIndex] = getBit(bmp, row, col++);
            }
        }
    }

    if (is_reversed_image(bmp))
    {
        reserve_data_frame(barCode);
    }
}

void reserve_data_frame(BarCode *barCode)
{
    for (int row = 0, col = START_POS_OF_EACH_ROW; row < barCode->nbRows; row++)
    {
        for (int frameIndex = 0; frameIndex < DFROW; frameIndex++)
        {
            for (int bitIndex = 0; 2 * bitIndex < DFCOL; bitIndex++)
            {
                barCode->barCodeRows[row].barCodeFrames[frameIndex].bin[bitIndex] =
                    barCode->barCodeRows[row].barCodeFrames[frameIndex].bin[DFCOL - bitIndex - 1];
            }
        }
    }
}

int parity_error[DFROW];
char how_to_run[] = ".\barcodeScanner.exe <file bitmap> [-d]";

void debugBarCode(char *fileName, Bmp bmp)
{
    printf("Read file %s\n", fileName);
    printf("Width: %d\n", bmp.width);
    printf("Height: %d\n", bmp.height);
}

bool frameValid(BarCodeFrame *barCodeFrame)
{
    bool msbParity = ((barCodeFrame->bin[1] + barCodeFrame->bin[2] - barCodeFrame->bin[3]) % 2 == 0);
    bool lsbParity = ((barCodeFrame->bin[4] + barCodeFrame->bin[5] - barCodeFrame->bin[6]) % 2 == 0);
    return msbParity && lsbParity;
}

bool check_error_a_frame(BarCode *barCode, int col)
{
    for (int row = 0; row < barCode->nbRows; row++)
    {
        BarCodeFrame frame = barCode->barCodeRows[row].barCodeFrames[col];
        if (frameValid(&frame))
            return true;
    }
    return false;
}

int get_barcode_value(BarCodeFrame *barCodeFrame)
{
    int code = 8 * barCodeFrame->bin[1] + 4 * barCodeFrame->bin[2] + 2 * barCodeFrame->bin[4] + barCodeFrame->bin[5];

    return code % 10;
}

bool check_error_BarCode(BarCode *barCode)
{

    for (int col = 0; col < DFCOL; col++)
    {
        if (check_error_a_frame(barCode, col) == false)
        {
            barCode->errorFrameIndices[barCode->nbErrorFrames++] = col;
        }
    }
    return (barCode->nbErrorFrames > 0);
}

bool validRow(BarCodeRow *barCodeRow)
{

    for (int frameIndex = 0; frameIndex < barCodeRow->nbFrames; frameIndex++)
    {
        if (!frameValid(&barCodeRow->barCodeFrames[frameIndex]))
            return false;
    }
    return true;
}

void get_barCode_values(BarCode *barCode)
{
    for (int row = 0; row < barCode->nbRows; row++)
        if (validRow(&barCode->barCodeRows[row]))
        {
            BarCodeRow barCodeRow = barCode->barCodeRows[row];

            for (int frameIndex = 0; frameIndex < barCodeRow.nbFrames; frameIndex++)
            {
                barCode->barCodeValues[frameIndex] = get_barcode_value(&barCodeRow.barCodeFrames[frameIndex]);
            }
            return;
        }
}

int main(int argc, char **argv)
{

    if (argc == 1)
    {
        printf("Misssing bitmap file!");
        printf("%s\n", how_to_run);
        return 0;
    }

    if (argc > 3)
    {
        printf("Too many arguments! At most 2 arguments");
        printf("%s\n", how_to_run);
        return 0;
    }

    char *filename = argv[1];
    char *flag = argv[argc - 1];
    Bmp bmp = read_bmp(filename);

    if (strcmp(flag, "-d") == 0)
    {
        debugBarCode(filename, bmp);
        return 0;
    }

    int data_frame[bmp.height][DFROW][DFCOL];
    BarCode barCode;

    get_data_frame(&barCode, &bmp);

    if (check_error_BarCode(&barCode))
    {
        printf("Unable to read frame: ");
        for (int i = 0; i < barCode.nbErrorFrames; i++)
            printf("%d ", barCode.errorFrameIndices[i]);
        return 0;
    }
    else
    {

        get_barCode_values(&barCode);
        for (int i = 0; i < barCode.NBFRAMES; i++)
        {
            printf("%d", barCode.barCodeValues[i]);
        }
    }
}