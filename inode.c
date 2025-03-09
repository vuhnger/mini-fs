#include "inode.h"
#include "block_allocation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Switch this to 0 to avoid cluttering terminal with print statements
#define DEBUG_MODE 0

// MAX ID must be incremented AFTER use 
static int MAX_ID = 0;

void debug(const char* function_name, const char* message, const char* optional_string) {
    if (DEBUG_MODE){
        fprintf(stderr, "[DEBUG] %s: %s %s\n", function_name, message, optional_string);
    }
}

void free_all_file_blocks(struct inode* node)
{
    if (!node || !node->entries)
        return;

    for (uint32_t i = 0; i < node->num_entries; i++) {
        free_block(node->entries[i]);
    }
}

void free_node(struct inode* node){
    if (!node){
        return;
    }
    if (node->is_directory){
        for (uint32_t i = 0; i < node->num_entries; i++){
            free_node((struct inode*) node->entries[i]);
        }
    } else {
        free_all_file_blocks(node);
    }
    free(node->entries);
    free(node->name);
    free(node);
}

struct inode* create_inode(
    uint32_t id,
    char* name,
    char is_directory,
    char is_readonly,
    uint32_t filesize,
    uint32_t num_entries,
    uintptr_t* entries
){
    struct inode* node = malloc(sizeof(struct inode));
    if (!node) {
        debug(__func__, "failed to allocate memory for new node", "");
        return NULL;
    }
    
    node->id = id;
    node->name = name;
    node->is_directory = is_directory;
    node->is_readonly = is_readonly;
    node->filesize = filesize;
    node->num_entries = num_entries;
    node->entries = entries;
    char node_info[100];
    snprintf(node_info, sizeof(node_info), 
             "Node(id=%u, name=%s, dir=%d, readonly=%d, size=%u, entries=%u)", 
             id, name, is_directory, is_readonly, filesize, num_entries);
    debug(__func__, "created node: ", node_info);
    return node;
}

struct inode* create_file(struct inode* parent, const char* name, char readonly, int size_in_bytes)
{
    debug(__func__, "attempting to create file:", name);

    if (!parent){
        debug(__func__, "parent pointer was NULL", "");
        return NULL;
    }

    if (!parent->is_directory){
        debug(__func__, "parent pointer is not a dir", "");
        return NULL;
    }

    // Check if file name exists
    if (find_inode_by_name(parent, name)){
        debug(__func__, "entry with (name) already exists", name);
        return NULL;
    }

    char* new_file_name = strdup(name);
    if (!new_file_name){
        debug(__func__, "failed to allocate memory for file name", "");
        return NULL;
    }

    int blocks_needed = (size_in_bytes + 4095) / 4096;

    struct inode* node = create_inode(MAX_ID, new_file_name, 0, readonly, size_in_bytes, blocks_needed, NULL);
    ++MAX_ID;

    // allocate entries
    node->entries = malloc(sizeof(uintptr_t) * blocks_needed);
    if (!node->entries){
        debug(__func__, "failed to allocate memory for new file","");
        free_node(node); // this frees node->name and node
        return NULL;
    }

    // allocate blocks
    for (int i = 0; i < blocks_needed; i++){
        int block = allocate_block(1);
        if (block == -1){
            debug(__func__, "failed to allocate memory for block", "");
            free_block(1);
            free_node(node);
            return NULL;
        }
        node->entries[i] = block;
    }

    // Safely realloc parent's entries
    uintptr_t* temp = realloc(parent->entries, sizeof(uintptr_t) * (parent->num_entries + 1));
    if (!temp) {
        debug(__func__, "failed to reallocate memory in parent directory", "");
        // We still own parent->entries if realloc fails
        free_node(node);
        return NULL;
    }
    parent->entries = temp;

    // add new file to parent's entries
    parent->entries[parent->num_entries] = (uintptr_t) node;
    parent->num_entries++;

    debug(__func__, "created file: ", name);
    return node;
}

struct inode* create_dir(struct inode* parent, const char* name)
{
    debug(__func__, "attempting directory creation: ", name);

    char* new_dir_name = strdup(name);
    if (!new_dir_name){
        debug(__func__, "failed to allocate memory for directory name", "");
        return NULL;
    }

    // If no parent => root
    if (!parent){
        debug(__func__, "parent pointer was NULL", "");
        struct inode* node = create_inode(MAX_ID, new_dir_name, 1, 0, 0, 0, NULL);
        MAX_ID++;
        if (!node){
            debug(__func__, "failed to create root node", "");
            --MAX_ID;
            free(new_dir_name);
            return NULL;
        }
        return node;
    } 

    if (!parent->is_directory){
        debug(__func__, "parent pointer is not a directory", "");
        free(new_dir_name);
        return NULL;  
    }

    // Check if exists
    if (find_inode_by_name(parent, name)){
        debug(__func__, "entry with (name) already exists", name);
        free(new_dir_name);
        return NULL;
    }

    // Realloc parent's entries
    parent->num_entries++;
    uintptr_t* tmp = realloc(parent->entries, sizeof(uintptr_t) * parent->num_entries);
    if (!tmp){
        --parent->num_entries;
        debug(__func__, "memory allocation for new_entries failed", "");
        free(new_dir_name);
        return NULL;
    }
    parent->entries = tmp;

    // create the node
    struct inode* node = create_inode(MAX_ID, new_dir_name, 1, 0, 0, 0, NULL);
    ++MAX_ID;
    if (!node){
        free(new_dir_name);
        --parent->num_entries;
        --MAX_ID;
        debug(__func__, "memory allocation for new_node failed", "");
        return NULL;
    }

    parent->entries[parent->num_entries - 1] = (uintptr_t) node;

    debug(__func__, "created directory: ", name);
    return node;
}

struct inode* find_inode_by_name(struct inode* parent, const char* name)
{
    if (!parent || !parent->is_directory) {
        return NULL;
    }

    for (int i = 0; i < parent->num_entries; i++){
        struct inode* child = (struct inode*)parent->entries[i];
        if (strcmp(child->name, name) == 0){
            fprintf(stderr, "name:%s\nchild name:%s\n", child->name, name);
            return child;
        }
    }
    return NULL;
}

int delete_file(struct inode* parent, struct inode* node)
{
    if (!parent || !node) {
        debug(__func__, "aborting file deletion: invalid ptrs", "");
        return -1;
    }
    if (node->is_directory) {
        debug(__func__, "aborting file deletion: node is directory", node->name);
        return -1;
    }
    if (!parent->is_directory) {
        debug(__func__, "aborting file deletion: parent not directory", "");
        return -1;
    }

    int file_index = -1;
    for (int i = 0; i < parent->num_entries; i++) {
        if ((uintptr_t)node == parent->entries[i]) {
            file_index = i;
            break;
        }
    }
    if (file_index == -1) {
        debug(__func__, "aborting file deletion: file not found", "");
        return -1;
    }

    // free blocks
    for (int i = 0; i < node->num_entries; i++) {
        int result = free_block(node->entries[i]);
        if (result == -1) {
            debug(__func__, "warning: failed to free block", "");
            // not returning is optional, but might leak
        }
    }

    // shift array down
    for (int i = file_index; i < parent->num_entries - 1; i++){
        parent->entries[i] = parent->entries[i + 1];
    }
    parent->num_entries--;

    // free the inode
    free_node(node);

    // safe realloc
    if (parent->num_entries > 0) {
        uintptr_t* tmp = realloc(parent->entries, parent->num_entries * sizeof(uintptr_t));
        if (!tmp){
            debug(__func__, "failed to reallocate memory for entry array", "");
            // parent->entries remains valid if tmp is NULL
            return -1;
        }
        parent->entries = tmp;
    } else {
        // if no entries remain, we can free or keep it as a 0-length array
        free(parent->entries);
        parent->entries = NULL;
    }

    debug(__func__, "file deleted successfully", "");
    return 0;
}

int delete_dir(struct inode* parent, struct inode* node)
{
    if (!node){
        debug(__func__, "aborting dir deletion: node was null", "");
        return -1;
    }
    if (!node->is_directory) {
        debug(__func__, "aborting dir deletion: node is not directory", node->name);
        return -1;
    }

    // If node is root
    if (!parent){
        free_node(node);
        debug(__func__, "freeing root directory", "");
        return 0;
    }

    if (!parent->is_directory) {
        debug(__func__, "aborting dir deletion: parent is not directory", "");
        return -1;
    }

    // Recurse on children
    for (uint32_t i = 0; i < node->num_entries; i++){
        struct inode* child = (struct inode*) node->entries[i];
        if (child->is_directory) {
            delete_dir(node, child);
        } else {
            delete_file(node, child);
        }
    }
    // Do not forget to remove 'node' from parent's entry array
    // (Similar approach to delete_file)

    // Find index in parent's array
    int dir_index = -1;
    for (int i = 0; i < parent->num_entries; i++){
        if ((struct inode*)parent->entries[i] == node){
            dir_index = i;
            break;
        }
    }
    if (dir_index == -1) {
        debug(__func__, "dir not found in parent->entries", node->name);
        return -1;
    }

    // shift array
    for (int i = dir_index; i < parent->num_entries - 1; i++){
        parent->entries[i] = parent->entries[i + 1];
    }
    parent->num_entries--;

    free_node(node); // now node and its contents are freed

    // safe realloc
    if (parent->num_entries > 0) {
        uintptr_t* tmp = realloc(parent->entries, parent->num_entries * sizeof(uintptr_t));
        if (!tmp) {
            debug(__func__, "failed to reallocate parent's entries array", "");
            return -1;
        }
        parent->entries = tmp;
    } else {
        free(parent->entries);
        parent->entries = NULL;
    }
    return 0;
}

void save_inodes(const char *master_file_table, struct inode *root)
{
    fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
}

struct inode *load_inodes(const char *master_file_table) 
{
    FILE *file = fopen(master_file_table, "rb");
    if (!file) {
        debug(__func__, "failed to open file:", master_file_table);
        return NULL;
    }

    struct inode *root = NULL;
    struct inode **inode_map = NULL;
    size_t inode_count = 0;

    while (1) {
        uint32_t id, name_length, filesize, num_entries;
        char is_directory, is_readonly;
        char *name;
        uintptr_t *entries;

        size_t bytesRead = fread(&id, sizeof(uint32_t), 1, file); 
        if (bytesRead != 1) {
            break;
        }
        if (id > MAX_ID) {
            MAX_ID = id;
        }

        fread(&name_length, sizeof(uint32_t), 1, file);
        name = malloc(name_length);
        if (!name){
            debug(__func__, "failed to allocate memory for name", "");
            fclose(file);
            return NULL;
        }
        fread(name, sizeof(char), name_length, file);
        fread(&is_directory, sizeof(char), 1, file);
        fread(&is_readonly,  sizeof(char), 1, file);

        if (!is_directory) {
            fread(&filesize, sizeof(uint32_t), 1, file);
        } else {
            filesize = 0;
        }

        fread(&num_entries, sizeof(uint32_t), 1, file);
        entries = malloc(num_entries * sizeof(uintptr_t));
        if (!entries){
            debug(__func__, "failed to allocate memory for entries", "");
            free(name);
            fclose(file);
            return NULL;
        }
        fread(entries, sizeof(uintptr_t), num_entries, file);

        debug(__func__, "loading inode", name);
        struct inode *node = create_inode(id, name, is_directory, is_readonly, filesize, num_entries, entries);

        // Safe reallocation for the inode_map
        if (id >= inode_count) {
            struct inode** tmp_map = realloc(inode_map, (id + 1) * sizeof(struct inode *));
            if (!tmp_map){
                debug(__func__, "failed to allocate memory for inode map", "");
                // We still have inode_map
                free(node);  // partial cleanup
                free(entries);
                free(name);
                fclose(file);
                return NULL;
            }
            inode_map = tmp_map;
            memset(inode_map + inode_count, 0, (id + 1 - inode_count) * sizeof(struct inode *));
            inode_count = id + 1;
        }
        inode_map[id] = node;

        if (id == 0) {
            root = node;
        }
    }

    fclose(file);

    // Fix up directory pointers
    for (size_t i = 0; i < inode_count; i++) {
        struct inode *node = inode_map[i];
        if (!node || !node->is_directory) continue;
        for (size_t j = 0; j < node->num_entries; j++) {
            node->entries[j] = (uintptr_t)inode_map[node->entries[j]];
        }
    }

    free(inode_map);
    return root;
}

void fs_shutdown(struct inode* inode)
{
    if (!inode) {
        return;
    }

    if (inode->is_directory) {
        for (uint32_t i = 0; i < inode->num_entries; i++) {
            fs_shutdown((struct inode*)inode->entries[i]);
        }
    }

    free(inode->name);
    free(inode->entries);
    free(inode);
}

/* Debug FS */
static int indent = 0;
static void debug_fs_print_table( const char* table );
static void debug_fs_tree_walk( struct inode* node, char* table );

void debug_fs( struct inode* node )
{
    char* table = calloc( NUM_BLOCKS, 1 );
    debug_fs_tree_walk( node, table );
    debug_fs_print_table( table );
    free( table );
}

static void debug_fs_tree_walk( struct inode* node, char* table )
{
    if (!node) return;
    for (int i=0; i<indent; i++)
        printf("  ");
    if (node->is_directory) {
        printf("%s (id %d)\n", node->name, node->id );
        indent++;
        for (int i=0; i<node->num_entries; i++){
            struct inode* child = (struct inode*)node->entries[i];
            debug_fs_tree_walk( child, table );
        }
        indent--;
    } else {
        printf("%s (id %d size %d)\n", node->name, node->id, node->filesize );

        /* Ugly extents approach (left as-is) */
        uint32_t* extents = (uint32_t*)node->entries;
        for (int i=0; i<node->num_entries; i++){
            for (int j=0; j<extents[2*i+1]; j++){
                table[ extents[2*i]+j ] = 1;
            }
        }
    }
}

static void debug_fs_print_table(const char* table)
{
    printf("Blocks recorded in master file table:");
    for (int i=0; i<NUM_BLOCKS; i++){
        if (i % 20 == 0) printf("\n%03d: ", i);
        printf("%d", table[i]);
    }
    printf("\n\n");
}