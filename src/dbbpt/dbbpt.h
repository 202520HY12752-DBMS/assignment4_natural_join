/*
 *  bpt.c
 */
#define Version "1.16.1"
/*
 *
 *  bpt:  B+ Tree Implementation
 *
 *  Copyright (c) 2018  Amittai Aviram  http://www.amittai.com
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.

 *  3. The name of the copyright holder may not be used to endorse
 *  or promote products derived from this software without specific
 *  prior written permission.

 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.

 *  Author:  Amittai Aviram
 *    http://www.amittai.com
 *    amittai.aviram@gmail.com or afa13@columbia.edu
 *  Original Date:  26 June 2010
 *  Last modified: 02 September 2018
 *
 *  This implementation demonstrates the B+ tree data structure
 *  for educational purposes, includin insertion, deletion, search, and display
 *  of the search path, the leaves, or the whole tree.
 *
 *  Must be compiled with a C99-compliant C compiler such as the latest GCC.
 *
 *  Usage:  bpt <filepath> [<input_filepath>]
 *  Filepath argument is mandatory to specify the file to store the B+ tree,
 *  input_filepath is optional to specify the file to read initial input from.
 *
 */
#include "file_manager.h"

// APIs
/**
 * @brief Find a record with the given key.
 * @param fd[in] The file descriptor of the database file.
 * @param key[in] The key to find.
 * @param verbose[in] Whether to print verbose output.
 * @param leaf_out[out] The leaf page pointer where the key is found.
 * @return The record with the given key, or NULL if not found.
 */
record *db_find(int fd, int64_t key, bool verbose, page **leaf_out);

/**
 * @brief Insert a record with the given key and value.
 * @param fd[in] The file descriptor of the database file.
 * @param key[in] The key to insert.
 * @param value[in] The value to insert.
 */
void db_insert(int fd, int64_t key, char *value);

/**
 * @brief Delete a record with the given key.
 * @param fd[in] The file descriptor of the database file.
 * @param key[in] The key to delete.
 */
void db_delete(int fd, int64_t key);

/**
 * @brief Destroy the database file.
 * @param fd[in] The file descriptor of the database file.
 */
void db_destroy(int fd);

/**
 * @brief Join two database files into a new output file.
 * @param fd1[in] The file descriptor of the first database file.
 * @param fd2[in] The file descriptor of the second database file.
 * @param output_filepath[in] The filepath of the output database file.
 */
void db_join(int fd1, int fd2, const char *output_filepath);


// Helper functions for find API
record *find1(int fd, int64_t root_pgn, int64_t key, bool verbose, page** leaf_out);
page *find_leaf(int fd, int64_t root_pgn, int64_t key, bool verbose);


// Helper functions for insertion API
void start_new_tree(int fd, header_page *header, int64_t key, char *value);
void insert_into_leaf(int fd, page *leaf, int64_t key, char *value);
void insert_into_leaf_after_splitting(int fd, header_page *header, page *leaf, int64_t key, char *value);
void insert_into_parent(int fd, header_page *header, page *left, int64_t key, page *right);
void insert_into_new_root(int fd, header_page *header, page *left, int64_t key, page *right);
void insert_into_page(int fd, page * n, int left_index, int64_t key, page * right);
void insert_into_page_after_splitting(
	  int fd, header_page *header, page *old_page, int left_index, int64_t key, page *right
);
int get_left_index(page* parent, page* left);


// Helper functions for delete API
void delete_entry(int fd, header_page *header, page *p, int64_t key, int64_t child_pgn);
void remove_entry_from_leaf_page(int fd, page *p, int64_t key);
void remove_entry_from_internal_page(int fd, page *p, int64_t key, int64_t child_pgn);
int get_neighbor_index(int fd, page *p, page *parent);
void adjust_root(int fd, header_page *header,  page *root);
void coalesce_pages(int fd, header_page *header, page *p, page *neighbor, int neighbor_index, int64_t k_prime);
void redistribute_pages(int fd, header_page *header, page *p, page *neighbor, int neighbor_index, int k_prime_index, int64_t k_prime);


// Helper functions for destroy API
void destroy_pages(int fd, int64_t pgn);


// Common utility functions
int cut(int length);