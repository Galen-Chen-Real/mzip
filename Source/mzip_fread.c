#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(UNIX64)
#define off64_t off_t
#define fseeko64 fseeko
#define ftello64 ftello
#endif

//#define SECTION_SIZE ((off64_t)4*1024*1024*1024)
off64_t SECTION_SIZE;
#define READ_SIZE ((off64_t)1024)

int send_zip(char *name);
int recv_zip(char *name);
int copy_file(char *to, off64_t to_location, char *name, off64_t from_location, off64_t size);
int chg_section_num(off64_t file_size);
int get_section_num(char *name);
off64_t get_file_size(char *name);

int main(int argc, char *argv[])
{
	if(argc >= 3) {
		if(!strcmp(argv[1], "-s")) {
			int num;
			if(argc >= 4 && (num = atoi(argv[3])) && num > 0 && num <= 4) {
				SECTION_SIZE = ((off64_t)num*1024*1024*1024);
			} else {
				SECTION_SIZE = ((off64_t)4*1024*1024*1024);
			}
			return send_zip(argv[2]);
		} else if(!strcmp(argv[1], "-r")) {
			return recv_zip(argv[2]);
		}
	}

	printf("Usage:%s -s filename [size][G]\n"\
		"      %s -r filename", argv[0], argv[0]);
	return -1;

}

int send_zip(char *name)
{
	int res = 0;
	char *des_name;
	if(!(des_name = malloc(strlen(name) + 2))) {
		return -1;
	}
	strcpy(des_name, name);
	int des_name_len = strlen(des_name);
	des_name[des_name_len + 1] = '\0';

	off64_t file_size = get_file_size(name);
	if(file_size < 0) {
		free(des_name);
		return -1;
	}
	printf("File:%s, Size:%dB\n", name, file_size);

	int section_num = chg_section_num(file_size);
	if(section_num < 0) {
		free(des_name);
		return -1;
	}

	printf("Start decompose, please wait ...\n");
	int i;
	off64_t location;
	for(i = 0; i < section_num; i++) {
		des_name[des_name_len] = '0' + i;
		remove(des_name);
		location = SECTION_SIZE * i;
		if(SECTION_SIZE * (i + 1) <= file_size) {
			res = copy_file(des_name, 0, name, location, SECTION_SIZE);
		} else {
			res = copy_file(des_name, 0, name, location, file_size - location);
		}
	}

	if(res) {
		printf("ERROR!\n");
		while(i) {
			des_name[des_name_len - 1] = '0' + i;
			remove(des_name);
		}
	} else {
		printf("DONE!\n");
	}

	free(des_name);
	return res;
}

int copy_file(char *to, off64_t to_location, char *from, off64_t from_location, off64_t size)
{
	int res = 0;
	char *buf = malloc(READ_SIZE);
	if(buf == NULL) {
		return -1;
	}
	FILE *from_file = fopen(from, "rb");
	if(from_file == NULL || fseeko64(from_file, from_location, SEEK_SET)) {
		free(buf);
		return -1;
	}
	FILE *to_file = fopen(to, "ab+");
	if(to_file == NULL || fseeko64(to_file, to_location, SEEK_SET)) {
		fclose(from_file);
		free(buf);
		return -1;
	}

	while(size >= READ_SIZE) {
		if(fread(buf, READ_SIZE, 1, from_file) != 1 || fwrite(buf, READ_SIZE, 1, to_file) != 1) {
			fclose(to_file);
			fclose(from_file);
			free(buf);
			remove(to);
			return -1;
		}
		size -= READ_SIZE;
	}
	int ch;
	while(size >= 1) {
		if((ch = fgetc(from_file)) == EOF || fputc(ch, to_file) == EOF) {
			fclose(to_file);
			fclose(from_file);
			free(buf);
			remove(to);
			return -1;
		}
		size -= 1;
	}

	fclose(to_file);
	fclose(from_file);
	free(buf);
	return res;
}

int recv_zip(char *name)
{
	int res = 0;
	char *recv_name;
	if((recv_name = malloc(strlen(name) + 1)) == NULL) {
		return -1;
	}
	strcpy(recv_name, name);
	recv_name[strlen(recv_name) - 1] = '\0';

	int section_num = get_section_num(name);
	if(section_num <= 0) {
		free(recv_name);
		return -1;
	}
	printf("File:%s, Section:%d\n", name, section_num);

	int i;
	printf("Start compose, please wait ...\n");
	remove(recv_name);
	for(i = 0; i < section_num; i++) {
		name[strlen(name) - 1] = '0' + i;
		if(copy_file(recv_name, i * SECTION_SIZE, name, 0, get_file_size(name)) < 0) {
			res = -1;
			break;
		}
	}
	if(res) {
		printf("ERROR!\n");
	} else {
		printf("DONE!\n");
	}
	
	free(recv_name);
	return res;
}

int chg_section_num(off64_t file_size)
{
	int section_num = file_size / SECTION_SIZE;
	section_num += (section_num * SECTION_SIZE < file_size ? 1 : 0);
	if(section_num > 9) {
		return -1;
	}
	return section_num;
}

int get_section_num(char *name) 
{
	char ch = name[strlen(name) - 1];
	if(ch >= '0' && ch <= '9') {
		return ch - '0' + 1;
	} else {
		return -1;
	}
}

off64_t get_file_size(char *name)
{
	FILE *file;
	if((file = fopen(name, "r")) == NULL) {
		return -1;
	}

	fseeko64(file, 0L, SEEK_END);
	off64_t size = ftello64(file);
	fclose(file);

	return size;
}
