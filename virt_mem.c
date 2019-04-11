#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

#define ARGC_ERROR 1
#define FILE_ERROR 2
#define BUFLEN 256
#define FRAME_SIZE  256
#define PAGE_SIZE 256
#define PAGE_ENTRIES 256
#define FRAME_ENTRIES 256
#define PAGE_BITS 8
#define MAX_TLB_ENTRIES 16
#define MEMORY_SIZE (FRAME_ENTRIES * FRAME_SIZE)

int page_table[PAGE_ENTRIES];
int TLB[MAX_TLB_ENTRIES][2];
int num_of_page_faults = 0;
int num_of_TLB_hits = 0;
int num_of_addresses = 0;
int TLB_pointer_front = -1;
int TLB_pointer_back = -1;
int memory_index = 0;
char phys_memory[MEMORY_SIZE];

float fault_rate;
float hit_rate;

//----------------------functions-----------------------------

unsigned int getpage(size_t x) { 
	return (0xff00 & x) >> 8; 
}

unsigned int getoffset(unsigned int x) { 
	return (0xff & x); 
}

void getpage_offset(unsigned int x) {
	unsigned int page = getpage(x);
	unsigned int offset = getoffset(x);
	printf("x is: %u, page: %u, offset: %u, address: %u, paddress: %u\n", x, page, offset,
         (page << 8) | getoffset(x), page * 256 + offset);
}

void init_TLB(int num)	{
	for(int i = 0; i < MAX_TLB_ENTRIES; i++) {
		TLB[i][0] = -1;
		TLB[i][1] = -1;
    }
}

void init_page_table(int num)	{
	for(int i = 0; i < PAGE_ENTRIES; i++) {
    	page_table[i] = num;
    }
}

int check_page_table(int page_num)	{
	if (page_table[page_num] == -1) {
		num_of_page_faults++;
	}
	return page_table[page_num];
}

int check_TLB(int page_num)	{
	for (int i = 0; i < MAX_TLB_ENTRIES; i++) {
		if (TLB[i][0] == page_num) {
			num_of_TLB_hits++;
			return TLB[i][1];
		}
	}
    return -1;
}

void update_TLB(int page_num, int frame_num)	{
	if (TLB_pointer_front == -1) {
		TLB_pointer_front = 0;
		TLB_pointer_back = 0;

		TLB[TLB_pointer_back][0] = page_num;
		TLB[TLB_pointer_back][1] = frame_num;
	}
	else {
		TLB_pointer_front = (TLB_pointer_front + 1) % MAX_TLB_ENTRIES;
		TLB_pointer_back = (TLB_pointer_back + 1) % MAX_TLB_ENTRIES;

		TLB[TLB_pointer_back][0] = page_num;
		TLB[TLB_pointer_back][1] = frame_num;
	}
	return;
}

//-------------------------------main-------------------------

int main(int argc, const char * argv[]) {
	init_TLB(-1);
	init_page_table(-1);


	FILE* fadd = fopen("addresses.txt", "r");
	if (fadd == NULL) { 
		fprintf(stderr, "Could not open file: 'addresses.txt'\n");  
		exit(FILE_ERROR);  
	}

	FILE* fcorr = fopen("correct.txt", "r");
	if (fcorr == NULL) { 
		fprintf(stderr, "Could not open file: 'correct.txt'\n");  
		exit(FILE_ERROR);  
	}

    int back; 
	char* back_data;
	back = open("BACKING_STORE.bin", O_RDONLY);
	back_data = mmap(0, MEMORY_SIZE, PROT_READ, MAP_SHARED, back, 0);
	if(back_data == MAP_FAILED) {
		close(back);
		printf("Error mmapping BACKING_STORE.bin!");
		exit(EXIT_FAILURE);
	}

	char buf[BUFLEN];
	unsigned int page, offset, physical_address_temp, frame = 0;
	unsigned int logic_address;                  // read from file address.txt
	unsigned int virt_address, phys_address, value;  // read from file correct.txt

	while(fgets(buf, sizeof(buf), fadd))	{
		page, offset, physical_address_temp, frame, logic_address, virt_address, phys_address, value = 0;
		num_of_addresses++;
		logic_address = atoi(buf);
    	page = getpage(logic_address);
	    offset = getoffset(logic_address);
		frame = check_TLB(page);

		if (frame != -1) {	// TLB Search successful
		    physical_address_temp = frame + offset;
		    value = phys_memory[physical_address_temp];
		}
		else {	// TLB search unsuccessful, trying page table
		    frame = check_page_table(page);

		    if (frame != -1) {	// Page table search successful
		        physical_address_temp = frame + offset;
		        update_TLB(page, frame);
            	value = phys_memory[physical_address_temp];
	        }
	        else {		// Page table search unsuccessful, page fault
				fseek(fback, page * 256, SEEK_SET);
				fread(buf, sizeof(char), 256, fback);

                int page_address = page * PAGE_SIZE;

                if (memory_index != -1) {
                    memcpy(phys_memory + memory_index, back_data + page_address, PAGE_SIZE);

                    frame = memory_index;
                    physical_address_temp = frame + offset;
                    value = phys_memory[physical_address_temp];

                    page_table[page] = memory_index;
                    update_TLB(page, frame);

                    if (memory_index < MEMORY_SIZE - FRAME_SIZE) {
                        memory_index += FRAME_SIZE;
                    }
                    else {
                        memory_index = -1;
                    }
                }
				num_of_page_faults++;
				frame++;
            }
        }
    	printf("Virtual address: %5u Physical address: %5u Value: %d\n", logic_address, physical_address_temp, value);
	}
	fclose(fcorr);
	fclose(fadd);
  
	fault_rate = (float) num_of_page_faults / (float) num_of_addresses;
	hit_rate = (float) num_of_TLB_hits / (float) num_of_addresses;
	printf("\nPage Faults = %d\n", num_of_page_faults);
	printf("Page Fault Rate = %f\n", fault_rate);
	printf("TLB Hits = %d\n", num_of_TLB_hits);
	printf("TLB Hit Rate %f\n", hit_rate);


	printf("\n\t\t...done.\n");
	return 0;
}
