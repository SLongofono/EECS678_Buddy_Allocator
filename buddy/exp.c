/*
 * This file demonstrates the decidedly counterintuitive use of the provided
 * list data type.
 */

#include <stdio.h>
#include <stdlib.h>
#include "list.h"

struct list_member{

	struct list_head list;
	int value;
	const char* label;
};

void printlist(struct list_head* pos){
}

int main(void){
	
	// Init two list_heads
	struct list_head myLists[2];
	INIT_LIST_HEAD(&myLists[0]);
	INIT_LIST_HEAD(&myLists[1]);

	int ret = list_empty(&myLists[0]);
	printf("List 1 empty returned %d\n", ret);	
	ret = list_empty(&myLists[1]);
	printf("List 2 empty returned %d\n", ret);	
	
	// Declare the first member of our list
	struct list_member new1;
	new1.value = 1;

	// initialize list (next and prev point to self)
	INIT_LIST_HEAD(&new1.list);

	struct list_member new2;
	new2.value = 2;
	INIT_LIST_HEAD(&new2.list);
	
	struct list_member new3;
	new3.value = 3;
	INIT_LIST_HEAD(&new3.list);

	struct list_member new4;
	new4.value = 4;
	INIT_LIST_HEAD(&new4.list);

	// Add to lists
	list_add_tail(&new1.list, &myLists[0]);
	list_add_tail(&new2.list, &myLists[1]);

	ret = list_empty(&myLists[0]);
	printf("List 1 empty returned %d\n", ret);	
	ret = list_empty(&myLists[1]);
	printf("List 2 empty returned %d\n", ret);	
	
	struct list_member * current = list_entry(myLists[0].next, struct list_member, list);
	printf("List 1 contains entry with value %d\n", current->value);

	current = list_entry(myLists[1].next, struct list_member, list);
	printf("List 2 contains entry with value %d\n", current->value);

	// Flip them and check again
	
	list_move_tail(myLists[0].next, myLists[1].next);

	ret = list_empty(&myLists[0]);
	printf("List 1 empty returned %d\n", ret);	
	ret = list_empty(&myLists[1]);
	printf("List 2 empty returned %d\n", ret);	
	
	list_move_tail(myLists[1].prev, myLists[0].next);

	ret = list_empty(&myLists[0]);
	printf("List 1 empty returned %d\n", ret);	
	ret = list_empty(&myLists[1]);
	printf("List 2 empty returned %d\n", ret);	
		
	
	current = list_entry(myLists[0].next, struct list_member, list);
	printf("List 1 contains entry with value %d\n", current->value);

	current = list_entry(myLists[1].next, struct list_member, list);
	printf("List 2 contains entry with value %d\n", current->value);


	return 0;
}
