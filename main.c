#include <pthread.h>
#include <immintrin.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef char i8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;
typedef unsigned long u64;

#define PRINT_ERROR(cstring) write(STDERR_FILENO, cstring, sizeof(cstring) - 1)
#define HEADER_COLOR {127, 188, 217}

#pragma pack(1)
struct bmp_header
{
	// Note: header
	i8 signature[2]; // should equal to "BM"
	u32 file_size;
	u32 unused_0;
	u32 data_offset;

	// Note: info header
	u32 info_header_size;
	u32 width;				   // in px
	u32 height;				   // in px
	u16 number_of_planes;	   // should be 1
	u16 bit_per_pixel;		   // 1, 4, 8, 16, 24 or 32
	u32 compression_type;	   // should be 0
	u32 compressed_image_size; // should be 0
							   // Note: there are more stuff there but it is not important here
};

struct file_content
{
	i8 *data;
	u32 size;
};

struct file_content read_entire_file(char *filename)
{
	char *file_data = 0;
	unsigned long file_size = 0;
	int input_file_fd = open(filename, O_RDONLY);
	if (input_file_fd >= 0)
	{
		struct stat input_file_stat = {0};
		stat(filename, &input_file_stat);
		file_size = input_file_stat.st_size;
		file_data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_file_fd, 0);
		close(input_file_fd);
	}
	return (struct file_content){file_data, file_size};
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		PRINT_ERROR("Usage: decode <input_filename>\n");
		return 1;
	}
	struct file_content file_content = read_entire_file(argv[1]);
	if (file_content.data == NULL)
	{
		PRINT_ERROR("Failed to read file\n");
		return 1;
	}
	struct bmp_header *header = (struct bmp_header *)file_content.data;

	u32 bits = header->bit_per_pixel / 8;
	u8 *data = (u8 *)(file_content.data + header->data_offset);

	u8 *start = NULL;
	u8 color[3] = HEADER_COLOR;
	int error = 0;
	for (u32 y = 0; y < header->height - 7; y++)
	{
		for (u32 x = 0; x < header->width - 7; x++)
		{
			u8 *pixel = data + (y * header->width + x) * (bits);
			if (*(u32 *)pixel == *(u32 *)color)
			{
				error = 0;
				for (int j = 1; j < 7; j++)
				{
					u8 *pixel_h = data + ((y + 7) * header->width + x + j) * (bits);
					u8 *pixel_v = data + ((y + j) * header->width + x) * (bits);
					if (*(u32 *)pixel_h != *(u32 *)color || *(u32 *)pixel_v != *(u32 *)color)
					{
						error = 1;
						break;
					}
					// check upper left corner is different
					u8 *pixel_ul = data + ((y + j) * header->width + x + j) * (bits);
					if (*(u32 *)pixel_ul == *(u32 *)color)
					{
						error = 1;
						break;
					}
				}
				if (!error)
				{
					start = pixel;
					break;
				}
			}
		}
		if (start != NULL)
			break;
	}
	if (start == NULL)
		return (PRINT_ERROR("Failed to find header\n"), 1);
	// printf("Found header at %p\n", start);
	// printf("Header: %02x %02x %02x\n", start[0], start[1], start[2]);

	start += (header->width * 7) * (bits);

	u8 *len_pixel = start + (bits) * 7;
	// printf("Len pixel: %02x %02x %02x\n", len_pixel[0], len_pixel[1], len_pixel[2]);

	u32 len = len_pixel[0] + len_pixel[2];
	// printf("Len: %u\n", len);

	// int output_fd = open("output.bmp", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	// if (output_fd < 0)
	// {
	// 	PRINT_ERROR("Failed to open output file\n");
	// 	return 1;
	// }

	start -= (header->width * 2 - 2) * (bits);

	u32 i = 0;
	char message[511] = {0};
	u8 *current_start = start;

	while (i < len)
	{
		// Fetch bytes in blocks of 'bits * 6' (if bits = 1, use 6 bytes at once)
		for (u32 j = 0; j < 6 * bits && i < len; ++j)
		{
			char byte = current_start[j];
			if (byte != 0)
			{
				message[i++] = byte;
			}
			current_start[j] = 0; // Clear the value after reading
		}

		// Update position after processing 6 * bits
		current_start -= (header->width) * bits;
	}

	// write(output_fd, file_content.data, header->data_offset);
	// write(output_fd, data, header->width * header->height * (bits));
	write(STDOUT_FILENO, message, len);
	return 0;
}
