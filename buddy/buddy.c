/**
 * Buddy Allocator
 */


/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

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
#define MIN_ORDER 12	// Represents the power of 2 of the minimum block size in bytes
#define MAX_ORDER 20	// Represents the power of 2 of the maximum block size in bytes

#define PAGE_SIZE (1<<MIN_ORDER)	// Represents the size of a page in bytes

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


/**
 * @type block_t
 *
 * @details The block type represents one or more pages in a compact format.
 * It makes use of the Linux kernel linked list, so it includes the list_head
 * type as a handle into that system.
 */
typedef struct {


	/* Usage notes
	 *
	 * Every list member has a member called list_head, because the Linux
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

	// Is this page free
	int isFree;

} block_t;


/**************************************************************************
 * Global Variables
 **************************************************************************/

/* free lists, store structs representing pages in blocks of various orders */
struct list_head free_area[MAX_ORDER+1];

/*
 * memory area, no real use beyond the macros and initialization
 * In fact, this is only still here because it was included code...
 */
char g_memory[1<<MAX_ORDER];

/* block structures */
block_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];


/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/


/**************************************************************************
 * Private Definitions
 **************************************************************************/

// Helpers

// Merge two blocks, and move to the next highest order.
void merge(block_t *block, block_t *buddy);

// Print a graph of the current allocations
void print_free_area();

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

// Returns a pointer to the buddy block with the given address if it exists in
// the free_area, or NULL otherwise
block_t* find_block(char* addr, int order);


/**************************************************************************
 * Local Functions
 **************************************************************************/


/**
 * @brief Initialize a list of all possible addresses for the given MAX_ORDER,
 *		and set up a list element for the first address.
 */
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

		//block_t *temp = (block_t*)malloc(sizeof(block_t));

		//g_pages[i] = *temp;

		// Initialize this as a linked list element
		INIT_LIST_HEAD(&g_pages[i].list);

		// All start as free
		g_pages[i].isFree = 1;

		// All start in highest order
		g_pages[i].order = MAX_ORDER;

		// Address is increments of page size from start
		//(g_pages[i].address) = (g_memory + (i*PAGE_SIZE));
		g_pages[i].address = (char*)PAGE_TO_ADDR(i);
	
	}
	
	/* add the entire memory as a single free block */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);
		

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
	 * 	The only thing necessary to represent a block in memory is its
	 * 	starting address and how big it is as a whole(order).
	 * 	
	 * 	If we maintain this scheme, we need only move around the
	 * 	minimum number of block types (***NOT PAGE TYPES***).  The
	 * 	rubric and lecture materials are a pedagogical nightmare.
	 * 	Let's call one thing a page, and other things something else!
	 *
	 * 	After identifying the smallest order with a free block, along
	 * 	with the target order of the given allocation size, we can
	 * 	determine the number of splits for each.  
	 * */



#if USE_DEBUG
	printf("Attempting to allocate for size %d...\n", size);
#endif

	// Check that size is valid
	if(size > (1 << MAX_ORDER) || size < 0){
		printf("[ INVALID SIZE ERROR : MAX SIZE IS %d BYTES ]\n", (1 << MAX_ORDER));
		return NULL;
	}

	int num_splits = 0;
	int target_order = MAX_ORDER;
	int active_order = -1;

	// Shuffling list members
	block_t *lefty;
	block_t *righty;

#if USE_DEBUG
	printf("Allocation is not too big...\n");
#endif

	if(size <= (1 << MIN_ORDER)){
		target_order = MIN_ORDER;	
	}
	else{
		// While the size we are looking at, divided by two, is larger than
		// the allocation size...
		while(size <= (1 <<(target_order-1))){
		
			// Update order for allocation
			target_order--;
		}
	}

	//Starting from the target order, find the smallest free_area with
	//free blocks
	for(active_order = target_order; active_order <= MAX_ORDER; active_order++){
		if(NULL != find_free_block(active_order)){
			break;
		}
		else if(active_order == MAX_ORDER){
			printf("[ OUT OF MEMORY ERROR ]\n");
			return NULL;
		}
		else{

#if USE_DEBUG
			printf("Order %d has no free blocks...\n", active_order);	
#endif
		}
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

#if USE_DEBUG
		printf("Removing left half from current active list...\n");
		count_blocks(&free_area[active_order]);
#endif
		
	
		// Need to remove the left from this order
		list_del(&lefty->list);
		
		//count_blocks(&free_area[active_order]);

		while(num_splits > 0){

			// Determine the right half side start address from the left half.  Retrieve the
			// associated page from g_pages. Use the enxt lowest
			// order since we are breaking this downward
			char * right_addr = BUDDY_ADDR(lefty->address, (active_order-1));
			righty = &g_pages[ADDR_TO_PAGE(right_addr)];
			righty->order = active_order-1;
			righty->isFree = 1;
			righty->address = right_addr;
			
			
#if USE_DEBUG
			printf("Right half at order %d will have address %p\n", righty->order, right_addr);
			printf("Adding right half to next lowest order...\n");
			count_blocks(&free_area[active_order-1]);
#endif

			// Add the right half to the free_area of the next lowest order.
			list_add(&righty->list, &free_area[active_order-1]);

#if USE_DEBUG
			count_blocks(&free_area[active_order-1]);
#endif

			// Sanity Check:
			block_t * temp = list_entry(free_area[active_order-1].next, block_t, list);
			assert(temp->address == righty->address);

			active_order--;
			
			num_splits--;
		}

		assert(active_order == target_order);

		// By this point, active_order should equal target order.  All that
		// remains to do is to adjust the size of lefty, set it to in use, and
		// add it to the back of the current list

		lefty->isFree = 0;
		lefty->order = active_order;
		list_add_tail(&lefty->list, &free_area[active_order]);
	} // End if(0 == num_splits)

#if USE_DEBUG
	print_free_area();
#endif
	
	return lefty->address;
}


/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free, or no buddies exist.
 *
 * @param addr memory block address to be freed
 *
 */
void buddy_free(void *addr)
{
	int current_order;
	block_t *block = NULL;
	block_t *buddy = NULL;

	// Locate the block associate with the address passed in
	// if it does not exist, error out
	for(current_order=MIN_ORDER; current_order < MAX_ORDER; ++current_order){
		block = find_block(addr, current_order);
		if(NULL != block){
			break;	
		}
	}
#if USE_DEBUG

	if(NULL == block){
		printf("[ FREE ERROR: FREE ON FREE PAGE ]\n");

		return;
	}
	else{
		printf("FREEING BLOCK OF ORDER %d (%d bytes)\n", block->order, (1 << block->order));
		printf("LOCATED THE GIVEN BLOCK...\n");
	}
#endif

	// 	Merging follows the pattern:
	//
	// 	Search for a buddy at the current order.
	// 	
	// 	If the buddy is not free, this block remains here and is
	// 	marked as free.  Update its order and return.
	// 	
	//	If the buddy does not exist, we assume that we are done.  Free
	//	this block, adjust the order, and return.
	//
	// 	Otherwise, merge the two into the block at hand, change the
	// 	order, and add to the next list up. Repeat.
	//
	

	// Identify the buddy address which goes with the given address
	char* buddy_addr = (char*)BUDDY_ADDR(addr, block->order);

	// locate the block which begins with this address in the free_areas
	buddy = find_block(buddy_addr, block->order);

	while(NULL != buddy){

		if(1 == buddy->isFree){

#if USE_DEBUG
			printf("Found free buddy %p\n", buddy->address);
			printf("Removing from order %d\n", block->order);
#endif

			// Remove both blocks from the current free_area
			list_del(&buddy->list);
			list_del(&block->list);

			// Destroy the block with the larger address. Undangle it.
			if(block->address < buddy->address){
				buddy = NULL;
			}
			else{
				block = buddy;
				buddy = NULL;
			}
			
			// Update the order for this block and add it to that free_area
			if(block->order < MAX_ORDER){
				block->order++;
			}

#if USE_DEBUG
			printf("Adding merged block to order %d\n", block->order);
			count_blocks(&free_area[block->order]);
#endif

			// Add the merged block to the now higher order or
			// block sizes
			list_add(&block->list, &free_area[block->order]);

#if USE_DEBUG
			count_blocks(&free_area[block->order]);
#endif


			// Get a new buddy address
			buddy_addr = (char*)BUDDY_ADDR(block->address, block->order);

			// Search for another buddy
			buddy = find_block(buddy_addr, block->order);
		}
		else{

#if USE_DEBUG
			printf("Buddy %p is still busy, freeing block...\n", buddy->address);
#endif

			// Buddy is still in use, exit and return
			buddy = NULL;
			break;
		}

	}// End while(NULL != buddy)
	
	// Mark block as freed
	block->isFree = 1;

#if USE_DEBUG
	print_free_area();
#endif
	
}


/**
 * @brief print free pages in each order.
 *
 *
 * @note The output of this function is diff'd with the expected results file.
 * 	 Bad touch!
 */
void buddy_dump()
{
#if USE_DEBUG
	buddy_dump_verbose();
#endif
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			block_t * temp = list_entry(pos, block_t, list);
			if(temp->isFree == 1){
				cnt++;
			}
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}


/**
 * @brief Print a more useful and thorough dump of the free area
 *
 */
void buddy_dump_verbose(){
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		int total = 0;
		list_for_each(pos, &free_area[o]) {
			block_t * temp = list_entry(pos, block_t, list);
			total++;
			if(1 == temp->isFree){
				cnt++;
			}
		}
		printf("(%d/%d):%dK ", cnt, total, (1<<o)/1024);
	}
	printf("\n");
	
}


/*
 * @brief Merge a block with its buddy and move to the next highest order.
 *
 * @note This function was left in because it has weird scoping issues.
 *
 * TODO Figure out why changes herein are not persistent in the block_t types
 * passed in.
 */
void merge(block_t *block, block_t *buddy){
	// Remove both blocks from the current free_area
	list_del(&buddy->list);
	list_del(&block->list);

	// Destroy the block with the larger address. Undangle it.
	if(block->address < buddy->address){
		buddy = NULL;
	}
	else{
		block = buddy;
		buddy = NULL;
	}
			
	// Update the order for this block and add it to that free_area
	if(block->order < MAX_ORDER){
		block->order++;
	}

#if USE_DEBUG
	printf("Adding merged block to order %d\n", block->order);
	count_blocks(&free_area[block->order]);
#endif
	list_add(&block->list, &free_area[block->order]);
#if USE_DEBUG
	count_blocks(&free_area[block->order]);
#endif

}



/**
 * @brief Print free areas in a tabular format, similar to the buddy allocator
 * 		slides and examples
 */
void  print_free_area(){
	
	int i;
	for(i=MAX_ORDER; i >= MIN_ORDER; i--){
		struct list_head *pos;
		printf("Order %d, %d bytes\n", i, (1 << i));
		printf(" --------------------------------------------------------------- \n");
		printf(" | ");
		list_for_each(pos, &free_area[i]){
			block_t * temp = list_entry(pos, block_t, list);
			if(1 == temp->isFree){
				printf("%p, F", temp->address);
			}
			else{
				printf("%p, A", temp->address);	
			}
			printf(" | ");
		}
		printf("\n");
		printf(" --------------------------------------------------------------- \n");
	}
	
}


/**
 * @brief Count and report the number of blocks in a given free_area member
 */
void count_blocks(struct list_head* theList){
	
	int count = 0;
	struct list_head * pos;
	list_for_each(pos, theList){
		count++;
	}
	printf("The given list has %d block entries\n", count);
}


/**
 * @brief Print information for a given block.
 */
void print_block(block_t * block){
	printf("Block Summary: (order, address, isFree)->(%d, %p, %s)\n", block->order, block->address, block->isFree == 1 ? "FREE": "NOT FREE");
}


/**
 * @brief Locate and return a pointer to a free block in the given order among
 * 		the free_area, or NULL if no such block exists.
 */
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


/**
 * @brief Locate and return a pointer to the block with the given address
 * 		within the free area of the given order.  Return NULL if
 * 		no such block exists.
 */
block_t* find_block(char* addr, int order){

#if USE_DEBUG
	printf("Searching order %d for %p...\n", order, addr);
#endif

	block_t * ret;
	struct list_head *p;
	list_for_each(p, &free_area[order]){
		ret = list_entry(p, block_t, list);
		if(ret->address == addr){
#if USE_DEBUG
			printf("Found block with address %p!\n", ret->address);
#endif
			return ret;	
		}
#if USE_DEBUG
		else{
			printf("%p doesn't match target %p, moving on...\n", ret->address, addr);	
		}
#endif
	}

#if USE_DEBUG
	printf("BLOCK %p NOT FOUND at order %d...\n", addr, order);
#endif
	return NULL;
}
