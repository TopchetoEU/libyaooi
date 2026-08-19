// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

// Generic macros for adding and removing stuff from singly and doubly-linked lists
// Define macros of the form yoi_list_<type>_next(node) and yoi_list_<type>_slot(node) for each linked list kind
// Each type corresponds to a single logical linked list

#define yoi_dlist_add(type, head, node) \
	yoi_list_##type##_next(node) = (head); \
	yoi_list_##type##_slot(node) = &(head); \
\
	if (head) yoi_list_##type##_slot(head) = &yoi_list_##type##_next(node); \
	(head) = (node);
#define yoi_dlist_del(type, node) \
	if (yoi_list_##type##_next(node)) yoi_list_##type##_slot(yoi_list_##type##_next(node)) = yoi_list_##type##_slot(node); \
	*yoi_list_##type##_slot(node) = yoi_list_##type##_next(node);

#define yoi_list_add(type, head, node) \
	yoi_list_##type##_next(node) = (head); \
	(head) = (node);
#define yoi_list_del(type, node) \
	node = yoi_list_##type##_next(node);
