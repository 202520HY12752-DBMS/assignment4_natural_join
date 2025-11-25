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
	off_t cur_offset = 0;
	if (pread(fd, &(dest->free_pgn), 8, cur_offset) == -1) exit_with_err_msg("Error on loading free page number at header page.");
	cur_offset += 8;

	if (pread(fd, &(dest->root_pgn), 8, cur_offset) == -1) exit_with_err_msg("Error on loading root page number at header page.");
	cur_offset += 8;

	if (pread(fd, &(dest->num_pages), 8, cur_offset) == -1) exit_with_err_msg("Error on loading number of pages at header page.");
	cur_offset += 8;

	if (pread(fd, &(dest->leaf_order), 4, cur_offset) == -1) exit_with_err_msg("Error on loading leaf order at header page.");
	cur_offset += 4;

	if (pread(fd, &(dest->internal_order), 4, cur_offset) == -1) exit_with_err_msg("Error on loading internal order at header page.");
	cur_offset += 4;
}

void write_header_page(int fd, const header_page* src) {
	off_t cur_offset = 0;
	if (pwrite(fd, &(src->free_pgn), 8, cur_offset) < 8) exit_with_err_msg("Error on writing free page number at header page.");
	cur_offset += 8;

	if (pwrite(fd, &(src->root_pgn), 8, cur_offset) < 8) exit_with_err_msg("Error on writing root page number at header page.");
	cur_offset += 8;

	if (pwrite(fd, &(src->num_pages), 8, cur_offset) < 8) exit_with_err_msg("Error on writing number of pages at header page.");
	cur_offset += 8;

	if (pwrite(fd, &(src->leaf_order), 4, cur_offset) < 4) exit_with_err_msg("Error on writing leaf order at header page.");
	cur_offset += 4;

	if (pwrite(fd, &(src->internal_order), 4, cur_offset) < 4) exit_with_err_msg("Error on writing internal order at header page.");
	cur_offset += 4;
}

void load_page(int fd, int64_t pgn, page* dest) {
	off_t cur_offset = pgn * PAGE_SIZE;
	dest->pgn = pgn;

	if (pread(fd, &(dest->parent_pgn), 8, cur_offset) == -1) exit_with_err_msg("Error on loading parent page number.");
	cur_offset += 8;

	if (pread(fd, &(dest->is_leaf), 4, cur_offset) == -1) exit_with_err_msg("Error on loading is_leaf flag.");
	cur_offset += 4;

	if (pread(fd, &(dest->num_keys), 4, cur_offset) == -1) exit_with_err_msg("Error on loading num_keys.");
	cur_offset += (4 + 104); // num_keys size(4) + reserved size(104)

	int64_t last_8byte_of_header;
	if (pread(fd, &(last_8byte_of_header), 8, cur_offset) == -1) exit_with_err_msg("Error on loading last 8 bytes of header section.");
	cur_offset += 8;
	if (dest->is_leaf) dest->right_sibling_pgn = last_8byte_of_header;
    else dest->child_pgns[dest->num_keys] = last_8byte_of_header;

	for (int i = 0; i < dest->num_keys; i++) {
		if (pread(fd, &(dest->keys[i]), 8, cur_offset) == -1) exit_with_err_msg("Error on loading keys.");
		cur_offset += 8;
		if (dest->is_leaf) {
			if (pread(fd, dest->records[i].value, 120, cur_offset) == -1) exit_with_err_msg("Error on loading leaf record value.");
			cur_offset += 120;
		} else {
			if (pread(fd, &(dest->child_pgns[i]), 8, cur_offset) == -1) exit_with_err_msg("Error on loading child page number.");
			cur_offset += 8;
		}
	}
}

void write_page(int fd, const page* src) {
	off_t cur_offset = src->pgn * PAGE_SIZE;

	if (pwrite(fd, &(src->parent_pgn), 8, cur_offset) < 8) exit_with_err_msg("Error on writing parent page number.");
	cur_offset += 8;

	if (pwrite(fd, &(src->is_leaf), 4, cur_offset) < 4) exit_with_err_msg("Error on writing is_leaf flag.");
	cur_offset += 4;

	if (pwrite(fd, &(src->num_keys), 4, cur_offset) < 4) exit_with_err_msg("Error on writing num_keys.");
	cur_offset += (4 + 104); // num_keys size(4) + reserved size(104)

	int64_t last_8byte_of_header;
	if (src->is_leaf) last_8byte_of_header = src->right_sibling_pgn;
	else last_8byte_of_header = src->child_pgns[src->num_keys];
	if (pwrite(fd, &(last_8byte_of_header), 8, cur_offset) < 8) exit_with_err_msg("Error on writing last 8 bytes of header section.");
	cur_offset += 8;

	for (int i = 0; i < src->num_keys; i++) {
		if (pwrite(fd, &(src->keys[i]), 8, cur_offset) < 8) exit_with_err_msg("Error on writing key.");
		cur_offset += 8;
		if (src->is_leaf) {
			if (pwrite(fd, src->records[i].value, 120, cur_offset) < 120) exit_with_err_msg("Error on writing leaf record value.");
			cur_offset += 120;
		} else {
			if (pwrite(fd, &(src->child_pgns[i]), 8, cur_offset) < 8) exit_with_err_msg("Error on writing internal record child page number.");
			cur_offset += 8;
		}
	}
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
