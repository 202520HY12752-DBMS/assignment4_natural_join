#ifndef __FILE_MANAGER_H__
#define __FILE_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#ifdef _WIN32
#define bool char
#define false 0
#define true 1
#endif


// Constants
#define MIN_LEAF_ORDER 3
#define MIN_INTERNAL_ORDER 3
#define MAX_LEAF_ORDER 32
#define MAX_INTERNAL_ORDER 249
#define DEFAULT_LEAF_ORDER 32
#define DEFAULT_INTERNAL_ORDER 249

#define INIT_PAGE_COUNT 4

#define PAGE_SIZE 4096
#define HEADER_PAGE_NUM 0


// Type definitions
typedef uint64_t pagenum_t;


// Structures
typedef struct record {
	char value[120];
} record;

typedef struct page {
	int64_t pgn;

	int64_t parent_pgn;
	bool is_leaf;
	int num_keys;
	int64_t keys[248];

	record records[31]; // for leaf page
	int64_t right_sibling_pgn; // for leaf page

	int64_t child_pgns[249]; // for internal page

	int64_t next_pgn; // for free page
} page;

typedef struct header_page {
	int64_t free_pgn;
	int64_t root_pgn;
	int64_t num_pages;

	// A maximum number of (key, value) pairs in the leaf page
	int leaf_order;
	// A maximum number of children in the internal node. The number of keys in the internal node is (internal_order - 1)
	int internal_order;
} header_page;


// APIs
/**
 * @brief Open a database file or create a new one if it doesn't exist.
 * @param file_path[in] The path to the database file.
 * @param leaf_order[in] The leaf order of the B+ tree.
 * @param internal_order[in] The internal order of the B+ tree.
 * @return The file descriptor of the database file. Return -1 if failed.
 *
 * Set the header page and free pages pool during tree creation. Plus, use -1
 * to represent a NULL page number for fields such as the initial root page number
 * in header page or next page number of the last free page.
 */
int open_or_create_tree(const char *file_path, int leaf_order, int internal_order);

/**
 * @brief Load the header page from the database file.
 * @param fd[in] The file descriptor of the database file.
 * @param dest[out] The destination to store the header page.
 */
void load_header_page(int fd, header_page* dest);

/**
 * @brief Write the header page to the database file.
 * @param fd[in] The file descriptor of the database file.
 * @param src[in] The header page to write.
 */
void write_header_page(int fd, const header_page* src);

/**
 * @brief Load a page from the database file.
 * @param fd[in] The file descriptor of the database file.
 * @param pgn[in] The page number of the page to load.
 * @param dest[out] The destination to store the page.
 */
void load_page(int fd, int64_t pgn, page* dest);

/**
 * @brief Write a page to the database file.
 * @param fd[in] The file descriptor of the database file.
 * @param src[in] The page to write. Return NULL if failed.
 */
void write_page(int fd, const page* src);

/**
 * @brief Allocate a new page.
 * @param fd[in] The file descriptor of the database file.
 * @return The new page.
 *
 * If no free pages are left, this function doubles the free pages pool, and then allocates one.
 * If memory allocation fails, then kill the process using the `exit_with_err_msg()` function.
 */
page *alloc_page(int fd);

/**
 * @brief Allocate a new page. This function uses the passed header_page pointer instead of calling load_header_page.
 * @param fd[in] The file descriptor of the database file.
 * @param header_page[in] The header page.
 * @return The new page.
 *
 * If no free pages are left, this function doubles the free pages pool, and then allocates one.
 * If memory allocation fails, then kill the process using the `exit_with_err_msg()` function.
 */
page *alloc_page1(int fd, header_page* header);

/**
 * @brief Free a page.
 * @param fd[in] The file descriptor of the database file.
 * @param pgn[in] The page number of the page to free.
 */
void free_page(int fd, int64_t pgn);


// Helper functions
void exit_with_err_msg(const char* err_msg);

#endif /* __FILE_MANAGER_H__ */
