/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 1

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12
#define MAX_ORDER 20

#define PAGE_SIZE (1<<MIN_ORDER)
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif


/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	/* Every list member has a member called list_head, because the Linux
	 * kernel uses a circular linked list scheme.
	 *
	 * list_head is used internally, and we need only to include it in our
	 * struct and initialize it to be able to use a linked list.
	 *
	 * When an individual element is initialized, its next and previous
	 * pointer are set to itself.
	 */
		


	struct list_head list;

	// The address where this page begins
	char* address;

	// The power of 2 representing the number of bytes in this block
	int order;

	// The index of this page into memory
	int index;

	// Is this page free
	int isFree;

} block_t;


/**************************************************************************
 * Global Variables
 **************************************************************************/

/* free lists, store structs representing pages in blocks of various orders */
struct list_head free_area[MAX_ORDER+1];

/* memory area, no real use beyond the macros and initialization */
char g_memory[1<<MAX_ORDER];

/* page structures */
block_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];

/* keeps track of allocated blocks in compact format */
struct list_head allocated;

	
// For managing the pages to be allocated and split
block_t buffer;


/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/


/**************************************************************************
 * Private Definitions
 **************************************************************************/
// Helpers

// Count and report number of block_t elements in the free_area of the
// specified order
void count_blocks(struct list_head * theList);

// Print block information for a specific block
void print_block(block_t * pg);

// Locate and return a pointer to the first free block in a given order for
// free_area, or NULL if no such block exists.
block_t* find_free_block(int order);

// Print block information for all free_area members, along with counts of
// each size block among all possible sizes
void buddy_dump_verbose();

// Print counts of each size block among all possible sizes in free_area
void buddy_dump();

/**************************************************************************
 * Local Functions
 **************************************************************************/

void buddy_init()
{

#if USE_DEBUG
	printf("Initializing buddy allocator...\n");
#endif

	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {
		INIT_LIST_HEAD(&free_area[i]);
	}

	for (i = 0; i < n_pages; i++) {

		block_t *temp = (block_t*)malloc(sizeof(block_t));

		g_pages[i] = *temp;

		// Initialize this as a linked list element
		INIT_LIST_HEAD(&g_pages[i].list);

		// Track indices for sizing and moving
		g_pages[i].index = i;

		// All start as free
		g_pages[i].isFree = 1;

		// All start in highest order
		g_pages[i].order = MAX_ORDER;

		// Address is increments of page size from start
		//(g_pages[i].address) = (g_memory + (i*PAGE_SIZE));
		g_pages[i].address = (char* )PAGE_TO_ADDR(i);
		
	}
	
	/* add the entire memory as a free block */
	list_add(&g_pages[0].list, free_area[MAX_ORDER].next);
		

#if USE_DEBUG
	printf("Done\n");
#endif

}


/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then split into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further split while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{

	/*
	 * Basic idea:
	 * 	The only thing you need to represent a chunk in memory is its
	 * 	starting address and how big it is as a whole.
	 * 	
	 * 	If we maintain this scheme, we need only move around the
	 * 	minimum number of block types (***NOT PAGE TYPES***).
	 *
	 * 	After identifying the smallest order with a free chunk, along
	 * 	with the target order of the given allocation size, we can
	 * 	determine the number of splits for each.  
	 * */



#if USE_DEBUG

	printf("Attempting to allocate for size %d...\n", size);
#endif
	// Check that size is valid
	assert(size <= (1 << MAX_ORDER) && size > 0);

	int num_splits = 0;
	int target_order = MAX_ORDER;
	int active_order = MAX_ORDER;

	// Navigating the linked lists
	block_t *lefty;
	block_t *righty;

#if USE_DEBUG
	printf("Allocation is not too big...\n");
#endif

	// While the size we are looking at, divided by two, is larger than
	// the allocation size...
	while(size <= (1 <<(target_order-1))){
		
		// Track the last order which had free pages
		// list_empty returns 0 if the list is NOT empty
		if(NULL == find_free_block(active_order)){
#if USE_DEBUG
			printf("Order %d had no free pages...\n", active_order);	
#endif
			active_order--;
		}

		// Update order for allocation
		target_order--;
	}

#if USE_DEBUG
	printf("Settled on order %d (%d bytes) for size %d...\n", target_order, (1<<target_order), size);
#endif

	// Make sure that we have free memory to allocate the requested size
	assert((1 << active_order) >= size);

#if USE_DEBUG
	printf("We have enough memory to perform the allocation...\n");
	printf("The smallest block size with free pages is order %d (%d bytes)\n", active_order, (1 << active_order));
#endif

	// Determine how many splits need to take place
	num_splits = active_order - target_order;

	// Retrieve the first empty block
	lefty = find_free_block(active_order);
	
	assert(NULL != lefty);
	

	if(0 == num_splits){

		// Since we are already at the target order, simply mark the
		// first free entry as taken and return its address.
		lefty->isFree = 0;
	}
	else{
		printf("Removing left half from current active list...\n");
		//count_blocks(&free_area[active_order]);
		
		
		// Need to remove the left from this order
		list_del(&lefty->list);
		
		//count_blocks(&free_area[active_order]);

		while(num_splits > 0){

			// Determine the right half side start address from the left half.  Retrieve the
			// associated page from g_pages.
			char * right_addr = BUDDY_ADDR(lefty->address, active_order);
		
			//righty = list_entry(&g_pages[ADDR_TO_PAGE(right_addr)].list, block_t, list);
			
			righty = &g_pages[ADDR_TO_PAGE(right_addr)];
			
			righty->order = active_order-1;
			righty->isFree = 1;
			righty->address = right_addr;

			
			printf("Adding right half to next lowest order...\n");
			//count_blocks(&free_area[active_order-1]);


			// Add the right half to the free_area of the next lowest order.
			list_add(&righty->list, free_area[active_order-1].next);

			//count_blocks(&free_area[active_order-1]);

			// Sanity Check:
			block_t * temp = list_entry(free_area[active_order-1].next, block_t, list);
			assert(temp->address == righty->address);

			active_order--;
			
			num_splits--;
		}

		// By this point, active_order should equal target order.  All that
		// remains to do is to adjust the size of lefty, set it to in use, and
		// add it to the back of the current list

		lefty->isFree = 0;
		lefty->order = active_order;
		list_add_tail(&lefty->list, free_area[active_order].next);
	}

	return lefty->address;
}


/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	/* TODO: IMPLEMENT THIS FUNCTION */
}


/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 *
 *
 * Note: this implies that free area lists are made up of pages and not
 * chunks.  For each size, it counts the number of pages, and divides by the
 * size of the current free_area list order
 */
void buddy_dump()
{
	buddy_dump_verbose();
	/*
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, free_area[o].next) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
	*/
}


/**
 * Print addresses of free area list members
 */
void  print_free_area(int order){
	
	int i;
	for(i=MIN_ORDER; i < MAX_ORDER; ++i){
		struct list_head *pos;
		list_for_each(pos, &free_area[i]){
			block_t * temp = list_entry(pos, block_t, list);
			print_block(temp);
		}
		printf("\n");
	}
}


void count_blocks(struct list_head* theList){
	
	int count = 0;
	struct list_head * pos;
	list_for_each(pos, theList){
		count++;
	}
	printf("The given list has %d block entries\n", count);
}


void print_block(block_t * block){
	printf("Block Summary: (order, address, isFree)->(%d, %p, %s)\n", block->order, block->address, block->isFree == 1 ? "FREE": "NOT FREE");
}


void init_block(block_t* block, char* addr, int order){
	block->address = addr;
	block->isFree = 1;
	block->order = order;
	INIT_LIST_HEAD(&block->list);
}


block_t* find_free_block(int order){

	block_t * ret;
	struct list_head * p;
	list_for_each(p, &free_area[order]){
		ret = list_entry(p, block_t, list);

		if(1 == ret->isFree){
			return ret;	
		}
	}
	return NULL;
}

void buddy_dump_verbose(){
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			block_t * temp = list_entry(pos, block_t, list);
			if(1 == temp->isFree){
				cnt++;
			}
			//printf("Block %d: (size, isFree)->(%d, %s)\n", cnt, (1 << temp->order), temp->isFree == 1 ? "FREE":"ALLOCATED");
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
	
}
