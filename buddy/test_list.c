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
	
	static LIST_HEAD(myList);

	int ret = list_empty(&myList);
	printf("List empty returned %d\n", ret);	
	


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


	// Add to list
	list_add_tail(&new1.list, &myList);
	list_add_tail(&new2.list, &myList);

	ret = list_empty(&myList);
	printf("List empty returned %d\n", ret);	

	// print list
	// Declare an iterator
	struct list_head *p;
	struct list_member *current;
	list_for_each(p, &myList){
		// Signature is (iterator, list type, name of list_head in
		// list type)
		current = list_entry(p, struct list_member, list);
		printf("The current element has value %d\n", current->value);
	}

	// Find and delete 2
	struct list_head * target;
	list_for_each(p, &myList){
		current = list_entry(p, struct list_member, list);
		if(current->value == 2){
			printf("Found element with value 2\n");
			target = p;
			// Free memory here!
		}
	}

	printf("Deleting...\n");

	// All this does is set next->Prev to prev, prev->next to next 
	list_del(target);


	// Print list again
	list_for_each(p, &myList){
		// Signature is (iterator, list type, name of list_head in
		// list type)
		current = list_entry(p, struct list_member, list);
		printf("The current element has value %d\n", current->value);
	}

	
	ret = list_empty(&myList);
	printf("List empty returned %d\n", ret);

	
	// Fetch and trim the front item
	target = myList.next;
	current = list_entry(target, struct list_member, list);
	
	// Remove from front
	printf("Deleting the front member, value = %d\n", current->value);
	
	list_del(target);

	ret = list_empty(&myList);
	printf("List empty returned %d\n", ret);


	// Add to list
	list_add_tail(&new1.list, &myList);
	list_add_tail(&new2.list, &myList);
	list_add_tail(&new3.list, &myList);
	list_add_tail(&new4.list, &myList);

	// print list
	list_for_each(p, &myList){
		current = list_entry(p, struct list_member, list);
		printf("The current element has value %d\n", current->value);
	}
	
	// Remove last entry
	target = myList.prev;
	current = list_entry(target, struct list_member, list);
	printf("Deleting rear element: %d\n", current->value);
	
	list_del(target);
	// list now has 1,2,3

	// print list
	list_for_each(p, &myList){
		current = list_entry(p, struct list_member, list);
		printf("The current element has value %d\n", current->value);
	}

	// Remove middle entry
	int iter  = 0;
	int count = 0;
	list_for_each(p, &myList){
		count++;
	}
	list_for_each(p, &myList){
		if(iter == count/2){
			target = p;
		}
		iter++;
	}

	current = list_entry(target, struct list_member, list);
	printf("Deleting middle element: %d\n", current->value);
	list_del(target);
	// list now has 1,3
	
	list_for_each(p, &myList){
		current = list_entry(p, struct list_member, list);
		printf("The current element has value %d\n", current->value);
	}

	return 0;
}
