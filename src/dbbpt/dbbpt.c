#include "file_manager.h"
#include "dbbpt.h"

#include <stdbool.h>
#ifdef _WIN32
#define bool char
#define false 0
#define true 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FUNCTION DEFINITIONS.
// Find API
record *db_find(int fd, int64_t key, bool verbose, page **leaf_out) {
	header_page header;
	load_header_page(fd, &header);
	return find1(fd, header.root_pgn, key, verbose, NULL);
}

// Helper functions for find API
record *find1(int fd, int64_t root_pgn, int64_t key, bool verbose, page **leaf_out) {
  page *leaf = find_leaf(fd, root_pgn, key, verbose);
	if (leaf == NULL) return NULL;

	int i = 0;
	for (i = 0; i < leaf->num_keys; i++) if (leaf->keys[i] == key) break;
	if (leaf_out != NULL) *leaf_out = leaf;

	if (i == leaf->num_keys) return NULL;
	else return &(leaf->records[i]);
}

page *find_leaf(int fd, int64_t root_pgn, int64_t key, bool verbose) {
	if (root_pgn <= 0) {
		if (verbose) printf("Empty tree.\n");
		return NULL;
	}

	page *cur_page = (page *)malloc(sizeof(page));
	load_page(fd, root_pgn, cur_page);
	while (!cur_page->is_leaf) {
		if (verbose) {
			printf("[");
			for (int i= 0; i < cur_page->num_keys - 1; i++) printf("%ld ", cur_page->keys[i]);
			printf("%ld] ", cur_page->keys[cur_page->num_keys - 1]);
		}

		int target_index;
		for (target_index = 0; target_index < cur_page->num_keys; target_index++) {
			if (key < cur_page->keys[target_index]) break;
		}
		if (verbose) printf("%d ->\n", target_index);

		load_page(fd, cur_page->child_pgns[target_index], cur_page);
	}

	if (verbose) {
		printf("Leaf [");
		for (int i = 0; i < cur_page->num_keys - 1; i++) printf("%ld ", cur_page->keys[i]);
		printf("%ld] ", cur_page->keys[cur_page->num_keys - 1]);
	}
	return cur_page;
}

// Insertion API
void db_insert(int fd, int64_t key, char *value) {
	header_page header;
	load_header_page(fd, &header);

	page *leaf = NULL;
	record *record = find1(fd, header.root_pgn, key, false, &leaf);
	if (record != NULL) {
		strcpy(record->value, value);
		write_page(fd, leaf);
		free(leaf);
		return;
	}

	if (header.root_pgn == -1) start_new_tree(fd, &header, key, value);
	else if (leaf->num_keys < header.leaf_order - 1) insert_into_leaf(fd, leaf, key, value);
	else insert_into_leaf_after_splitting(fd, &header, leaf, key, value);

	free(leaf);
}

// Helper functions for insertion API
void start_new_tree(int fd, header_page *header, int64_t key, char *value) {
	page *root = alloc_page1(fd, header);

	root->is_leaf = true;
	root->parent_pgn = -1;
	root->right_sibling_pgn = -1;
	root->num_keys = 1;
	root->keys[0] = key;
	strcpy(root->records[0].value, value);
	write_page(fd, root);

	header->root_pgn = root->pgn;
  write_header_page(fd, header);

	free(root);
}

void insert_into_leaf(int fd, page *leaf, int64_t key, char *value) {
	int insertion_point = 0;
	while (insertion_point < leaf->num_keys && leaf->keys[insertion_point] < key) {
		insertion_point += 1;
	}

	for (int i = leaf->num_keys; i > insertion_point; i--) {
		leaf->keys[i] = leaf->keys[i - 1];
		strcpy(leaf->records[i].value, leaf->records[i - 1].value);
	}
	leaf->keys[insertion_point] = key;
	strcpy(leaf->records[insertion_point].value, value);
	leaf->num_keys += 1;
	write_page(fd, leaf);
}

void insert_into_leaf_after_splitting(int fd, header_page *header, page *leaf, int64_t key, char *value) {
	int64_t *temp_keys = (int64_t *)malloc((header->leaf_order) * sizeof(int64_t));
	if (temp_keys == NULL) exit_with_err_msg("Error on allocating temporary keys array.");

	char **temp_values = (char **)malloc((header->leaf_order) * sizeof(char *));
	for (int i = 0; i < header->leaf_order; i++) {
		temp_values[i] = (char *)malloc(120 * sizeof(char));
		if (temp_values[i] == NULL) exit_with_err_msg("Error on allocating temporary values array.");
	}
	if (temp_values == NULL) exit_with_err_msg("Error on allocating temporary values array.");

	int insertion_index = 0;
	while (insertion_index < header->leaf_order - 1 && leaf->keys[insertion_index] < key) {
		insertion_index += 1;
	}

	for (int i = 0, j = 0; i < leaf->num_keys; i++, j++) {
		if (j == insertion_index) j += 1;
		temp_keys[j] = leaf->keys[i];
		strcpy(temp_values[j], leaf->records[i].value);
	}
	temp_keys[insertion_index] = key;
	strcpy(temp_values[insertion_index], value);

	leaf->num_keys = 0;
	int split = cut(header->leaf_order - 1);
	for (int i = 0; i < split; i++) {
		leaf->keys[i] = temp_keys[i];
		strcpy(leaf->records[i].value, temp_values[i]);
		leaf->num_keys += 1;
	}

	page *new_leaf = alloc_page1(fd, header);
	new_leaf->is_leaf = true;
	new_leaf->num_keys = 0;
	for (int i = split, j = 0; i < header->leaf_order; i++, j++) {
		new_leaf->keys[j] = temp_keys[i];
		strcpy(new_leaf->records[j].value, temp_values[i]);
		new_leaf->num_keys += 1;
	}

	for (int i = 0; i < header->leaf_order; i++) free(temp_values[i]);
	free(temp_values);
	free(temp_keys);

  new_leaf->parent_pgn = leaf->parent_pgn;
	new_leaf->right_sibling_pgn = leaf->right_sibling_pgn;
	leaf->right_sibling_pgn = new_leaf->pgn;
	write_page(fd, new_leaf);
	write_page(fd, leaf);

	insert_into_parent(fd, header, leaf, new_leaf->keys[0], new_leaf);
	free(new_leaf);
}

void insert_into_parent(int fd, header_page *header, page *left, int64_t key, page *right) {
	if (left->parent_pgn == -1) return insert_into_new_root(fd, header, left, key, right);

	page parent;
	load_page(fd, left->parent_pgn, &parent);

	int left_index = get_left_index(&parent, left);
	if (parent.num_keys < header->internal_order - 1) insert_into_page(fd, &parent, left_index, key, right);
	else insert_into_page_after_splitting(fd, header, &parent, left_index, key, right);
}

void insert_into_new_root(int fd, header_page *header, page *left, int64_t key, page *right) {
	page *new_root = alloc_page1(fd, header);
	new_root->parent_pgn = -1;
	new_root->is_leaf = false;
	new_root->keys[0] = key;
	new_root->child_pgns[0] = left->pgn;
	new_root->child_pgns[1] = right->pgn;
	new_root->num_keys = 1;
	write_page(fd, new_root);

	left->parent_pgn = new_root->pgn;
	right->parent_pgn = new_root->pgn;
	write_page(fd, left);
	write_page(fd, right);

	header->root_pgn = new_root->pgn;
	write_header_page(fd, header);

	free(new_root);
}

void insert_into_page(int fd, page *p, int left_index, int64_t key, page *new_child) {
	for (int i = p->num_keys; i > left_index; i--) {
		p->keys[i] = p->keys[i - 1];
		p->child_pgns[i + 1] = p->child_pgns[i];
	}
	p->child_pgns[left_index + 1] = new_child->pgn;
	p->keys[left_index] = key;
	p->num_keys += 1;
	write_page(fd, p);

	new_child->parent_pgn = p->pgn;
	write_page(fd, new_child);
}

void insert_into_page_after_splitting(int fd, header_page *header, page *old_page, int left_index, int64_t key, page *right) {
	int64_t *temp_keys = (int64_t *)malloc(header->internal_order * sizeof(int64_t));
	if (temp_keys == NULL) exit_with_err_msg("Error on allocating temporary keys array.");

	int64_t *temp_child_pgns = (int64_t *)malloc((header->internal_order + 1) * sizeof(int64_t));
	if (temp_child_pgns == NULL) exit_with_err_msg("Error on allocating temporary child page numbers array.");

	for (int i = 0, j = 0; i < old_page->num_keys; i++, j++) {
		if (j == left_index) j++;
		temp_keys[j] = old_page->keys[i];
	}

	for (int i = 0, j = 0; i < old_page->num_keys + 1; i++, j++) {
		if (j == left_index + 1) j++;
		temp_child_pgns[j] = old_page->child_pgns[i];
	}

	temp_child_pgns[left_index + 1] = right->pgn;
	temp_keys[left_index] = key;

	page *new_page = alloc_page1(fd, header);
	new_page->is_leaf = false;
	new_page->parent_pgn = old_page->parent_pgn;
	new_page->num_keys = 0;
	old_page->num_keys = 0;

	int split = cut(header->internal_order);
	int index;
	for (index = 0; index < split - 1; index++) {
		old_page->keys[index] = temp_keys[index];
		old_page->child_pgns[index] = temp_child_pgns[index];
		old_page->num_keys += 1;
	}
	old_page->child_pgns[index] = temp_child_pgns[index];
	index += 1;

	int new_page_index;
	for (new_page_index = 0; index < header->internal_order; index++, new_page_index++) {
		new_page->keys[new_page_index] = temp_keys[index];
		new_page->child_pgns[new_page_index] = temp_child_pgns[index];
		new_page->num_keys++;
	}
	new_page->child_pgns[new_page_index] = temp_child_pgns[index];
	write_page(fd, old_page);
	write_page(fd, new_page);

	for (int i = 0; i < new_page->num_keys + 1; i++) {
		page child;
		load_page(fd, new_page->child_pgns[i], &child);
		child.parent_pgn = new_page->pgn;
		write_page(fd, &child);
	}

	insert_into_parent(fd, header, old_page, temp_keys[split - 1], new_page);
	free(temp_child_pgns);
	free(temp_keys);
	free(new_page);
}

int get_left_index(page *parent, page *left) {
	int left_index = 0;
	while(left_index <= parent->num_keys && parent->child_pgns[left_index] != left->pgn) {
		left_index += 1;
	}
	return left_index;
}

// Deletion API
void db_delete(int fd, int64_t key) {
	header_page header;
	load_header_page(fd, &header);

	page *key_leaf = NULL;
	record *key_record = find1(fd, header.root_pgn, key, false, &key_leaf);

	if (key_record != NULL && key_leaf != NULL) delete_entry(fd, &header, key_leaf, key, -1);
	free(key_leaf);
}

// Helper functions for delete API
void delete_entry(int fd, header_page *header, page *p, int64_t key, int64_t child_pgn) {
	// Remove key and value(child) from page.
	if (p->is_leaf) remove_entry_from_leaf_page(fd, p, key);
	else remove_entry_from_internal_page(fd, p, key, child_pgn);

	if (p->pgn == header->root_pgn) return adjust_root(fd, header, p);
	int min_keys = p->is_leaf ? cut(header->leaf_order - 1) : cut(header->internal_order) - 1;
	if (p->num_keys >= min_keys) return;

	page parent;
	load_page(fd, p->parent_pgn, &parent);
	int neighbor_index = get_neighbor_index(fd, p, &parent);
	int k_prime_index = (neighbor_index == -1 ? 0 : neighbor_index);
	int64_t k_prime = parent.keys[k_prime_index];

	int64_t neighbor_pgn = (neighbor_index == -1 ? parent.child_pgns[1] : parent.child_pgns[neighbor_index]);
	page neighbor;
	load_page(fd, neighbor_pgn, &neighbor);

	int capacity = p->is_leaf ? header->leaf_order : header->internal_order - 1;
	if (neighbor.num_keys + p->num_keys < capacity) return coalesce_pages(fd, header, p, &neighbor, neighbor_index, k_prime);
	else return redistribute_pages(fd, header, p, &neighbor, neighbor_index, k_prime_index, k_prime);
}

void remove_entry_from_leaf_page(int fd, page *p, int64_t key) {
	bool need_move = false;
	for (int i = 0; i < p->num_keys; i++) {
		if (p->keys[i] == key) {
			i += 1;
			need_move = true;
		}
		if (!need_move) continue;
		if (i == p->num_keys) break;
		p->keys[i - 1] = p->keys[i];
		strcpy(p->records[i - 1].value, p->records[i].value);
	}
	p->num_keys--;
	write_page(fd, p);
}

void remove_entry_from_internal_page(int fd, page *p, int64_t key, int64_t child_pgn) {
	bool need_move = false;
	for (int i = 0; i < p->num_keys; i++) {
		if (p->keys[i] == key) {
			i += 1;
			need_move = true;
		}
		if (!need_move) continue;
		p->keys[i - 1] = p->keys[i];
	}

	need_move = false;
	for (int i = 0; i < p->num_keys + 1; i++) {
		if (p->child_pgns[i] == child_pgn) {
			i += 1;
			need_move = true;
		}
		if (!need_move) continue;
		p->child_pgns[i - 1] = p->child_pgns[i];
	}
	p->num_keys--;
	write_page(fd, p);
}

int get_neighbor_index(int fd, page *p, page *parent) {
	for (int i = 0; i < parent->num_keys + 1; i++) {
		if (parent->child_pgns[i] == p->pgn) return i - 1;
	}

	// Error state.
	printf("Search for nonexistent pointer to page in parent.\n");
	printf("page number:  %ld\n", p->pgn);
	exit(EXIT_FAILURE);
}

void adjust_root(int fd, header_page *header,  page *root) {
	if (root->num_keys > 0) return;

	int64_t new_root_pgn = -1;
	if (!root->is_leaf) {
		new_root_pgn = root->child_pgns[0];

		page new_root;
		load_page(fd, new_root_pgn, &new_root);
		new_root.parent_pgn = -1;
		write_page(fd, &new_root);
	}
	header->root_pgn = new_root_pgn;
	write_header_page(fd, header);

	free_page(fd, root->pgn);
}

void coalesce_pages(int fd, header_page *header, page *p, page *neighbor, int neighbor_index, int64_t k_prime) {
	page* survivor = neighbor;
	page* deleted = p;
	if (neighbor_index == -1) {
		survivor = p;
		deleted = neighbor;
	}

	int appended_index = survivor->num_keys;
	if (survivor->is_leaf) {
		for (int i = appended_index, j = 0; j < deleted->num_keys; i++, j++) {
			survivor->keys[i] = deleted->keys[j];
			strcpy(survivor->records[i].value, deleted->records[j].value);
			survivor->num_keys++;
		}
		survivor->right_sibling_pgn = deleted->right_sibling_pgn;
	} else {
		survivor->keys[appended_index] = k_prime;
		survivor->num_keys++;

		int deleted_end = deleted->num_keys;
		for (int i = appended_index + 1, j = 0; j < deleted_end; i++, j++) {
			survivor->keys[i] = deleted->keys[j];
			survivor->child_pgns[i] = deleted->child_pgns[j];
			survivor->num_keys += 1;
			deleted->num_keys -= 1;
		}
		survivor->child_pgns[survivor->num_keys] = deleted->child_pgns[deleted_end];

		for (int i = appended_index + 1; i < survivor->num_keys + 1; i++) {
			page child;
			load_page(fd, survivor->child_pgns[i], &child);
			child.parent_pgn = survivor->pgn;
			write_page(fd, &child);
		}
	}
	write_page(fd, survivor);

	page parent;
	load_page(fd, survivor->parent_pgn, &parent);
	delete_entry(fd, header, &parent, k_prime, deleted->pgn);

	free_page(fd, deleted->pgn);
}

void redistribute_pages(int fd, header_page *header, page *p, page *neighbor, int neighbor_index,
		int k_prime_index, int64_t k_prime) {
	page parent;
	load_page(fd, p->parent_pgn, &parent);
	if (neighbor_index != -1) {
		if (p->is_leaf) {
			for (int i = p->num_keys; i > 0; i--) {
				p->keys[i] = p->keys[i - 1];
				strcpy(p->records[i].value, p->records[i - 1].value);
			}
			p->keys[0] = neighbor->keys[neighbor->num_keys - 1];
			strcpy(p->records[0].value, neighbor->records[neighbor->num_keys - 1].value);

			parent.keys[k_prime_index] = p->keys[0];
		} else {
			p->child_pgns[p->num_keys + 1] = p->child_pgns[p->num_keys];
			for (int i = p->num_keys; i > 0; i--) {
				p->keys[i] = p->keys[i - 1];
				p->child_pgns[i] = p->child_pgns[i - 1];
			}
			p->keys[0] = k_prime;
			p->child_pgns[0] = neighbor->child_pgns[neighbor->num_keys];

			parent.keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];

			page moved_child;
			load_page(fd, p->child_pgns[0], &moved_child);
			moved_child.parent_pgn = p->pgn;
			write_page(fd, &moved_child);
		}
	}	else {
		if (p->is_leaf) {
			p->keys[p->num_keys] = neighbor->keys[0];
			strcpy(p->records[p->num_keys].value, neighbor->records[0].value);

			parent.keys[k_prime_index] = neighbor->keys[1];
			for (int i = 0; i < neighbor->num_keys - 1; i++) {
				neighbor->keys[i] = neighbor->keys[i + 1];
				strcpy(neighbor->records[i].value, neighbor->records[i + 1].value);
			}
		} else {
			p->keys[p->num_keys] = k_prime;
			p->child_pgns[p->num_keys + 1] = neighbor->child_pgns[0];

			parent.keys[k_prime_index] = neighbor->keys[0];
			for (int i = 0; i < neighbor->num_keys - 1; i++) {
				neighbor->keys[i] = neighbor->keys[i + 1];
				neighbor->child_pgns[i] = neighbor->child_pgns[i + 1];
			}
			neighbor->child_pgns[neighbor->num_keys - 1] = neighbor->child_pgns[neighbor->num_keys];

			page moved_child;
			load_page(fd, p->child_pgns[p->num_keys + 1], &moved_child);
			moved_child.parent_pgn = p->pgn;
			write_page(fd, &moved_child);
		}
	}
	p->num_keys += 1;
	neighbor->num_keys -= 1;

	write_page(fd, p);
	write_page(fd, neighbor);
	write_page(fd, &parent);
}

// Destroy API
void db_destroy(int fd) {
	header_page header;
	load_header_page(fd, &header);
	destroy_pages(fd, header.root_pgn);
	header.root_pgn = -1;
	write_header_page(fd, &header);
}

// Helper functions for destroy API
void destroy_pages(int fd, int64_t pgn) {
	if (pgn <= 0) return;
	page cur_page;
	load_page(fd, pgn, &cur_page);
	if (!cur_page.is_leaf) {
		for (int i = 0; i < cur_page.num_keys + 1; i++) destroy_pages(fd, cur_page.child_pgns[i]);
	}
	free_page(fd, pgn);
}

// Join API
void db_join(int fd1, int fd2, const char *output_filepath) {
	// TODO: Implement join function
}

// Common utility functions
int cut(int length) {
	if (length % 2 == 0)
		return length / 2;
	else
		return length / 2 + 1;
}
