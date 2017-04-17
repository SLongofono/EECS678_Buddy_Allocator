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
 * Private Definitions
 **************************************************************************/
// Helpers


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

	// The index of this page into memory
	int index;

	// Is this page free
	int isFree;

} page_t;


// Used to keep track of allocated groups of pages, so we can rebuild them
// when we free a block
typedef struct{

	struct list_head list;

	// Where does this block begin
	char* address;

	// What size is this block
	int order;
} alloc_block;


/**************************************************************************
 * Global Variables
 **************************************************************************/

/* free lists, store structs representing pages in blocks of various orders */
struct list_head free_area[MAX_ORDER+1];

/* memory area, no real use beyond the macros and initialization */
char g_memory[1<<MAX_ORDER];

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];

/* keeps track of allocated blocks in compact format */
struct list_head allocated;


/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/


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
	for (i = 0; i < n_pages; i++) {
		// Initialize this as a linked list element
		INIT_LIST_HEAD(&g_pages[i].list);

		// Track indices for sizing and moving
		g_pages[i].index = i;

		// Address is increments of page size from start
		//(g_pages[i].address) = (g_memory + (i*PAGE_SIZE));
		(g_pages[i].address) = (char*)PAGE_TO_ADDR(i);

		// All start as free
		g_pages[i].isFree = 1;
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {
		INIT_LIST_HEAD(&free_area[i]);
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);

	// Initialize the allocations list head pointer
	LIST_HEAD(allocated);

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

	printf("Attempting to allocate for size %d...\n", size);

	int num_splits = 0;
	int target_order = MAX_ORDER;
	int active_order = MAX_ORDER;
	int num_pages, remaining_pages;
	char *alloc_start_address = NULL;

	// Tracks the allocated block start address and order
	alloc_block alloc;

	// Navigating the linked lists
	struct list_head *cur_list_head;
	
	// For managing the pages to be allocated and split
	struct list_head *buffer;
	INIT_LIST_HEAD(buffer);

	// Check that size is valid (not too big)
	assert(size <= (1 << MAX_ORDER));
	printf("Allocation is not too big...\n");

	// While the size we are looking at, divided by two, is larger than
	// the allocation size...
	while(size <= (1 <<(target_order-1))){
		
		// Track the last order which had free pages
		if(!list_empty(&free_area[target_order-1])){
			active_order--;
			printf("Order %d had free pages...\n", active_order);
		}

		// Update order for allocation
		target_order--;
	}

	printf("Settled on order %d (%d bytes) for size %d...\n", target_order, (1<<target_order), size);

	// Make sure that we have free memory to allocate the requested size
	assert((1 << active_order) >= size);

	printf("We have enough memory to perform the allocation...\n");

	// Determine how many splits need to take place
	num_splits = active_order - target_order;

	/* Pull off pages to be allocated
	 *
	 */
	remaining_pages = ((1 << active_order)/PAGE_SIZE);

	cur_list_head = &free_area[active_order];

	// Gather all pages to be moved into the buffer in order
	while(remaining_pages > 0){
		// Add head of current free_area to rear of buffer
		list_add_tail(cur_list_head, buffer);

		// Remove current free area head
		list_del(cur_list_head);
		
		remaining_pages--;
	}
	
	// Create and store the allocation block
	cur_list_head = buffer->next;
	page_t* temp = list_entry(buffer, page_t, list);
	alloc.address = temp->address;
	alloc.order = target_order;
	list_add(&alloc.list, &allocated);


	// Performs splitting and shuffles around pages accordingly
	while(active_order >= target_order){
		active_order--;
		cur_list_head = &free_area[active_order];
		remaining_pages = (1 << active_order)/PAGE_SIZE;
		
		while(remaining_pages > 0){
			// Add rear of buffer to front of current free_area
			list_add(buffer->prev, cur_list_head);

			// Remove rear of buffer
			list_del(buffer->prev);

			remaining_pages--;
		}

	}


	//num_pages = ((1 << target_order)/PAGE_SIZE);

	
	//struct  page_t *p = list_entry
	
	//printf("pulling off member %p\n", p->address);
	
	// Split remaining pages among the other free_areas.  Need to start at
	// the bottom to maintain the proper page order.
	//while(num_splits > 0){
		

	//	next_list = &free_area[target_order];
	//	num_splits--;
	//}

	// By this point, target_order is where the allocation should happen.
	// Also, the order of the active_order-target_order is the number of
	// times to split
	

	

	// Check if a free block exists at the target size
	// If yes, got to allocation
	// If no
	//while(list_empty()){
			
	//}


	/*
	 * Basic algorithm
	 *
	 * 	Find smallest non empty list
	 *	
	 * 	Get head of that list.
	 *
	 * 	Determine if we need to split: condition is if half of the list
	 * 	size is greater than the allocation size
	 *
	 * 	Splitting algorithm (keep track of head of final list)
	 *
	 * 	Take the first block of the free area in use, create an alloc_block, 
	 * 	and assign the first address of the first page to that block
	 *
	 * 	Pop off that many pages
	 *
	 * 	Return the address of the allocated block.
	 *
	 * Splitting algorithm
	 *
	 *     find lowest free page size
	 *
	 *     if(lowest > size){
	 *
	 *     		find lowest size that will fit the page
	 *
	 *     		while((current_order >> 1) > size){
	 *			current_order >> 1;
	 *			cur_list_head = next_lowest_list
	 *     		}
	 *     	}
	 *     	else{
	 *		find minimum page size that will fit
	 *     	}
	 *
	 *     
	 *
	 *
	 * 	While half the size of the list members is greater than the size of of
	 * 	the desired allocation size:
	 *
	 * 		remove list size/page size pages from that list
	 * 		to the 
	 *
	 * */
	return NULL;
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

	/*
	 * Basic idea:
	 * 	The 
	 *
	 * */
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
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}


/**
 * Print addresses of free area list members
 */
void  print_free_area(order){
	int i;
	for(i=MIN_ORDER; i < MAX_ORDER; ++i){
		struct list_head *pos;
		list_for_each(pos, &free_area[i]){
			printf("%p, ", list_entry(pos, page_t, list)->address);	
		}
		printf("\n");
	}
}
