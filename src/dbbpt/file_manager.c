#include "file_manager.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int open_or_create_tree(const char *file_path, int leaf_order, int internal_order) {
	int fd = open(file_path, O_RDWR);

	if (fd > 0) return fd;

	if (leaf_order < MIN_LEAF_ORDER || leaf_order > MAX_LEAF_ORDER ||
			internal_order < MIN_INTERNAL_ORDER || internal_order > MAX_INTERNAL_ORDER) {
		printf("Invalid order\n");
		return -1;
	}
	// If the file does not exist, create it.
	fd = open(file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd == -1) exit_with_err_msg("Error creating file.");

	// Initialize the free pages for a new file.
	int64_t next_free_pgn = -1;
	for (int64_t i = 1; i < INIT_PAGE_COUNT; i++) {
		if (pwrite(fd, &next_free_pgn, 8, i * PAGE_SIZE) < 8) exit_with_err_msg("Error on creating initial free pages.");
		next_free_pgn = i;
	}

	// Initialize the header page for a new file.
	header_page header;
	header.root_pgn = -1; // No root page yet.
	header.num_pages = INIT_PAGE_COUNT;
	header.free_pgn = next_free_pgn;
	header.leaf_order = leaf_order;
	header.internal_order = internal_order;
	write_header_page(fd, &header);

	return fd;
}

void load_header_page(int fd, header_page* dest) {
	char buffer[PAGE_SIZE];
	if (pread(fd, buffer, PAGE_SIZE, 0) == -1) exit_with_err_msg("Error on loading header page.");
	
	int offset_on_pg = 0;
	memcpy(&(dest->free_pgn), buffer + offset_on_pg, 8);
	offset_on_pg += 8;
	memcpy(&(dest->root_pgn), buffer + offset_on_pg, 8);
	offset_on_pg += 8;
	memcpy(&(dest->num_pages), buffer + offset_on_pg, 8);
	offset_on_pg += 8;
	memcpy(&(dest->leaf_order), buffer + offset_on_pg, 4);
	offset_on_pg += 4;
	memcpy(&(dest->internal_order), buffer + offset_on_pg, 4);
	offset_on_pg += 4;
}

void write_header_page(int fd, const header_page* src) {
	char buffer[PAGE_SIZE];
	memset(buffer, 0, PAGE_SIZE);

	int offset_on_pg = 0;
	memcpy(buffer + offset_on_pg, &(src->free_pgn), 8);
	offset_on_pg += 8;
	memcpy(buffer + offset_on_pg, &(src->root_pgn), 8);
	offset_on_pg += 8;
	memcpy(buffer + offset_on_pg, &(src->num_pages), 8);
	offset_on_pg += 8;
	memcpy(buffer + offset_on_pg, &(src->leaf_order), 4);
	offset_on_pg += 4;
	memcpy(buffer + offset_on_pg, &(src->internal_order), 4);
	offset_on_pg += 4;

	if (pwrite(fd, buffer, PAGE_SIZE, 0) < PAGE_SIZE) exit_with_err_msg("Error on writing header page.");
}

void load_page(int fd, int64_t pgn, page* dest) {
	char buffer[PAGE_SIZE];
	if (pread(fd, buffer, PAGE_SIZE, pgn * PAGE_SIZE) == -1) exit_with_err_msg("Error on loading page.");
	int offset_on_pg = 0;
	
	dest->pgn = pgn;
	memcpy(&(dest->parent_pgn), buffer + offset_on_pg, 8);
	offset_on_pg += 8;
	memcpy(&(dest->is_leaf), buffer + offset_on_pg, 4);
	offset_on_pg += 4;
	memcpy(&(dest->num_keys), buffer + offset_on_pg, 4);
	offset_on_pg += (4 + 104); // num_keys size(4) + reserved size(104)

	int64_t last_8byte_of_header;
	memcpy(&last_8byte_of_header, buffer + offset_on_pg, 8);
	offset_on_pg += 8;

	if (dest->is_leaf) dest->right_sibling_pgn = last_8byte_of_header;
	else dest->child_pgns[dest->num_keys] = last_8byte_of_header;

	for (int i = 0; i < dest->num_keys; i++) {
		memcpy(&(dest->keys[i]), buffer + offset_on_pg, 8);
		offset_on_pg += 8;
		if (dest->is_leaf) {
			memcpy(dest->records[i].value, buffer + offset_on_pg, 120);
			offset_on_pg += 120;
		} else {
			memcpy(&(dest->child_pgns[i]), buffer + offset_on_pg, 8);
			offset_on_pg += 8;
		}
	}
}

void write_page(int fd, const page* src) {
	char buffer[PAGE_SIZE];
	memset(buffer, 0, PAGE_SIZE);

	int offset_on_pg = 0;
	memcpy(buffer + offset_on_pg, &(src->parent_pgn), 8);
	offset_on_pg += 8;
	memcpy(buffer + offset_on_pg, &(src->is_leaf), 4);
	offset_on_pg += 4;
	memcpy(buffer + offset_on_pg, &(src->num_keys), 4);
	offset_on_pg += (4 + 104); // num_keys size(4) + reserved size(104)

	int64_t last_8byte_of_header;
	if (src->is_leaf) last_8byte_of_header = src->right_sibling_pgn;
	else last_8byte_of_header = src->child_pgns[src->num_keys];
	memcpy(buffer + offset_on_pg, &last_8byte_of_header, 8);
	offset_on_pg += 8;

	for (int i = 0; i < src->num_keys; i++) {
		memcpy(buffer + offset_on_pg, &(src->keys[i]), 8);
		offset_on_pg += 8;
		if (src->is_leaf) {
			memcpy(buffer + offset_on_pg, src->records[i].value, 120);
			offset_on_pg += 120;
		} else {
			memcpy(buffer + offset_on_pg, &(src->child_pgns[i]), 8);
			offset_on_pg += 8;
		}
	}
	if (pwrite(fd, buffer, PAGE_SIZE, src->pgn * PAGE_SIZE) < PAGE_SIZE) exit_with_err_msg("Error on writing page.");
}

page *alloc_page(int fd) {
	header_page header;
	load_header_page(fd, &header);

	return alloc_page1(fd, &header);
}

page *alloc_page1(int fd, header_page *header_page) {
	int64_t cur_free_pgn = header_page->free_pgn;
	if (cur_free_pgn == -1) {
		// If there is no free page, make more free page.
		for (int64_t i = header_page->num_pages; i < header_page->num_pages * 2; i++) {
			if (pwrite(fd, &cur_free_pgn, 8, i * PAGE_SIZE) < 8) exit_with_err_msg("Error on creating initial free pages.");
			cur_free_pgn = i;
		}
		header_page->num_pages *= 2;
		header_page->free_pgn = cur_free_pgn;
	}

	int64_t next_pgn;
	if (pread(fd, &next_pgn, 8, cur_free_pgn * PAGE_SIZE) == -1) exit_with_err_msg("Error on reading next free page number.");
	header_page->free_pgn = next_pgn;
	write_header_page(fd, header_page);

	page *new_page = (page *)malloc(sizeof(page));
	if (new_page == NULL) exit_with_err_msg("Error on allocating new page.");
	new_page->pgn = cur_free_pgn;
	return new_page;
}

void free_page(int fd, int64_t pgn) {
	header_page header;
	load_header_page(fd, &header);

	int64_t cur_free_pgn = header.free_pgn;
	if (pwrite(fd, &cur_free_pgn, 8, pgn * PAGE_SIZE) < 8) exit_with_err_msg("Error on writing free page number to freed page.");

	header.free_pgn = pgn;
	write_header_page(fd, &header);
}

void exit_with_err_msg(const char* err_msg) {
	perror(err_msg);
	exit(EXIT_FAILURE);
}
