#pragma once

// Generic macros for adding and removing stuff from singly and doubly-linked lists
// Define macros of the form evi_list_<type>_next(node) and evi_list_<type>_slot(node) for each linked list kind
// Each type corresponds to a single logical linked list

#define evi_dlist_add(type, head, node) \
	evi_list_##type##_next(node) = (head); \
	evi_list_##type##_slot(node) = &(head); \
\
	if (head) evi_list_##type##_slot(head) = &evi_list_##type##_next(node); \
	(head) = (node);
#define evi_dlist_del(type, node) \
	if (evi_list_##type##_next(node)) evi_list_##type##_slot(evi_list_##type##_next(node)) = evi_list_##type##_slot(node); \
	*evi_list_##type##_slot(node) = evi_list_##type##_next(node);

#define evi_list_add(type, head, node) \
	evi_list_##type##_next(node) = (head); \
	(head) = (node);
#define evi_list_del(type, node) \
	node = evi_list_##type##_next(node);
