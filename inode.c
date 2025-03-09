#include "inode.h"
#include "block_allocation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Switch this to 0 to avoid cluttering terminal with print statements
#define DEBUG_MODE 1

// MAX ID must be incremented AFTER use 
static int MAX_ID = 0;

/* 
 * Prints a debug message with the function name.
 * 
 * @param function_name The name of the function calling debug.
 * @param message The debug message to print.
 * @param optional additional information, pass "" as default arg
 */
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
    }else{
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
    // Create the inode to be inserted
    struct inode* node = malloc(sizeof(struct inode));
    if (!node) {
        debug(__func__, "failed to allocate memory for new node", "");
        //free(node);
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


struct inode* create_file( struct inode* parent, const char* name, char readonly, int size_in_bytes )
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

    // Check if there already exists a file with the new name in the current directory
    if (find_inode_by_name(parent, name)){
        debug(__func__, "entry with (name) already exists", name);
        return NULL;
    }

    // Duplicate name to adjust the data type to match constructor of an inode
    char* new_file_name = strdup(name);
    if (!new_file_name){
        debug(__func__, "failed to allocate memory for file name", "");
        //free(new_file_name);
        return NULL;
    }

    struct inode* node;

    // Allocate memory for entries

    // Calculcate the number of blocks needed to store the file
    int blocks_needed = (size_in_bytes + 4095) / 4096;

    
    node = create_inode(MAX_ID, new_file_name,0,readonly,size_in_bytes,blocks_needed,NULL);
    ++MAX_ID;

    // Allocate memory for entries
    node->entries = malloc(sizeof(uintptr_t) * blocks_needed);
    if (!node->entries){
        debug(__func__, "failed to allocate memory for new file","");
        free_node(node);
        return NULL;
    }

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

    // Reallocate space for this file in parent dir entries
    parent->entries = realloc(parent->entries, sizeof(uintptr_t) * (parent->num_entries + 1));
    if(!parent->entries){
        debug(__func__, "failed to reallocate memory in parent directory", "");
        for (uint32_t i = 0; i < parent->num_entries; i++){
            free_node((struct inode*) parent->entries[i]);
        }
        // Maybe needed 
        //free(parent->entries);
        return NULL;
    }
    parent->num_entries++;

    parent->entries[parent->num_entries - 1] = (uintptr_t) node;

    debug(__func__, "created file: ", name);
    return node;
}

struct inode* create_dir( struct inode* parent, const char* name )
{
    debug(__func__, "attempting directory creation: ", name);

    struct inode* node;

    // Duplicate name to adjust the data type to match constructor of an inode
    char* new_dir_name = strdup(name);
    if (!new_dir_name){
        debug(__func__, "failed to allocate memory for directory name", "");
        free(new_dir_name);
        return NULL;
    }

    // Check if directory is root
    if (!parent){
        debug(__func__, "parent pointer was NULL", "");
        node = create_inode(MAX_ID, new_dir_name, 1,0,0,0,NULL);
        MAX_ID++;
        if (!node){
            free(node);
            debug(__func__, "failed to create root node", "");
            --MAX_ID;
            return NULL;
        }
        return node;
    } 

    if (!parent->is_directory){
        debug(__func__, "parent pointer is not a directory", "");
        return NULL;  
    }

    // Check if there already exists a directory or file with the new name in the current directory
    if (find_inode_by_name(parent, name)){
        debug(__func__, "entry with (name) already exists", name);
        return NULL;
    }

    // Allocate memory for new directory
    // Calculated as: size of current dir + size of new dir + 1
    
    // Check whether num_entries has been initialized
    // Update the number of entries to reflect a new dir added
    ++parent->num_entries;
    uintptr_t* new_entries = realloc(parent->entries, sizeof(uintptr_t) * parent->num_entries);
    //TODO: Must free the realloc if it fails
    
    if (!new_entries){
        // If memory reallocation fails, the count should not be increased because no dir was added
        --parent->num_entries;
        debug(__func__, "memory allocation for new_entries failed", "");
        return NULL;
    }
    parent->entries = new_entries;

    
    // Create the new node
    node = create_inode(MAX_ID,new_dir_name,1,0,0,0,NULL);
    
    // Increment the max id 
    ++MAX_ID;
    if (!node){
        free(new_dir_name);
        --parent->num_entries;
        --MAX_ID;
        debug(__func__, "memory allocation for new_node failed", "");
        return NULL;
    }
    // Add a pointer to the new dir from parent dir
    parent->entries[parent->num_entries - 1] = (uintptr_t) node;

    debug(__func__, "created directory: ", name);
    return node;

}



struct inode *find_inode_by_name(struct inode *parent, const char *name)
{

    if (!parent)
    {
        return NULL;
    }
    if (!(parent->is_directory))
    {
        return NULL;
    }

    for (int i = 0; i < parent->num_entries; i++)
    {
        struct inode *child = (struct inode *)parent->entries[i];
        if (strcmp(child->name, name) == 0)
        {
            fprintf(stderr, "name:%s\nchild name:%s\n", child->name, name);
            return child;
        }
    }

    // fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
    return NULL;
}

int delete_file(struct inode* parent, struct inode* node)
{
    
    if (!parent) {
        debug(__func__, "aborting file deletion: parent pointer was null", "");
        return -1;
    }
    if (!node) {
        debug(__func__, "aborting file deletion: file is null", "");
        return -1;
    }
    if (node->is_directory) {
        debug(__func__, "aborting file deletion: node is a directory", node->name);
        return -1;
    }
    if (!parent->is_directory) {
        debug(__func__, "aborting file deletion: parent is not a directory", "");
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
        debug(__func__, "aborting file deletion: file not found in parent directory", "");
        return -1;
    }

    
    for (int i = 0; i < node->num_entries; i++) {
        int result = free_block(node->entries[i]);
        if (result == -1) {
            debug(__func__, "warning: failed to free block", "");
            return -1;
        }
    }

    for (int i = file_index; i < parent->num_entries - 1; i++){
        parent->entries[i] = parent->entries[i + 1];
    }
    --parent->num_entries;
    
    free_node(node);

    uintptr_t *new_entries = realloc(parent->entries, parent->num_entries * sizeof(uintptr_t));
    if (!new_entries){
        debug(__func__, "failed to reallocate memory for entry array", "");
        //free(new_entries);
        return -1;
    }
    parent->entries = new_entries;

    debug(__func__, "file deleted successfully", "");
    return 0;
}

int delete_dir( struct inode* parent, struct inode* node )
{
    
    if (!node){
        debug(__func__, "aborting dir deletion: node was null", "");
        return -1;
    }
    // Check if the dir is root, in that case delete it
    if (!parent){
        free_node(node);
        debug(__func__, "freeing root directory", "");
        return 0;
    }

    if (!node->is_directory) {
        debug(__func__, "aborting dir deletion: node is a directory", node->name);
        return -1;
    }
    if (!parent->is_directory) {
        debug(__func__, "aborting dir deletion: parent is not a directory", "");
        return -1;
    }
    
    // Loop over all entries and call delete_file or delete_dir
    for (uint32_t i = 0; i < node->num_entries; i++){
        // Delete all files from a directory before calling delete_dir recursively to delete any files in subdirectories etc.
        uintptr_t child = node->entries[i];
        if (!((struct inode*) child)->is_directory){
            delete_file(node, (struct inode*) child);
        }else{
            delete_dir(node, (struct inode*) child);
        }
    }
    // Free all memory items related to the deleted node
    free_node(node);
    return 0;
}

void save_inodes(const char *master_file_table, struct inode *root)
{
    fprintf( stderr, "%s is not implemented\n", __FUNCTION__ );
}

struct inode *load_inodes(const char *master_file_table) {
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

        if (bytesRead != 1) 
            break;
        if(id > MAX_ID)
            MAX_ID = id;

        fread(&name_length, sizeof(uint32_t), 1, file);
        name = malloc(name_length);
        if (!name){
            debug(__func__, "failed to allocate memory for name", "");
            //free(name);
            return NULL;
        }
        fread(name, sizeof(char), name_length, file);
        fread(&is_directory, sizeof(char), 1, file);
        fread(&is_readonly, sizeof(char), 1, file);

        if (!is_directory) {
            fread(&filesize, sizeof(uint32_t), 1, file);
        } else {
            filesize = 0;
        }

        fread(&num_entries, sizeof(uint32_t), 1, file);
        entries = malloc(num_entries * sizeof(uintptr_t));
        if (!entries){
            debug(__func__, "failed to allocate memory for entries", "");
            //free(entries);
            return NULL;
        }
        fread(entries, sizeof(uintptr_t), num_entries, file);

        debug(__func__, "loading inode", name);
        struct inode *node = create_inode(id,name,is_directory,is_readonly,filesize,num_entries,entries);

        if (id >= inode_count) {
            inode_map = realloc(inode_map, (id + 1) * sizeof(struct inode *));
            if (!inode_map){
                free(inode_map);
                debug(__func__, "failed to allocate memory for inode map", "");
                return NULL;
            }
            memset(inode_map + inode_count, 0, (id + 1 - inode_count) * sizeof(struct inode *));
            inode_count = id + 1;
        }
        inode_map[id] = node;

        if (id == 0) {
            root = node;
        }
    }

    fclose(file);

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
    if (!inode)
    {
        return;
    }

    if (inode->is_directory)
    {
        for (uint32_t i = 0; i < inode->num_entries; i++)
        {
            fs_shutdown((struct inode*)inode->entries[i]);
        }
    }

    free(inode->name);
    free(inode->entries);
    free(inode);
}

/* This static variable is used to change the indentation while 
 * is walking through the tree of inodes and prints information.
 */
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
    if( node == NULL ) return;
    for( int i=0; i<indent; i++ )
        printf("  ");
    if( node->is_directory )
    {
        printf("%s (id %d)\n", node->name, node->id );
        indent++;
        for( int i=0; i<node->num_entries; i++ )
        {
            struct inode* child = (struct inode*)node->entries[i];
            debug_fs_tree_walk( child, table );
        }
        indent--;
    }
    else
    {
        printf("%s (id %d size %d)\n", node->name, node->id, node->filesize );

        /* The following is an ugly solution. We expect you to discover a
         * better way of handling extents in the node->entries array, and did
         * it like this because we don't want to give away a good solution here.
         */
        uint32_t* extents = (uint32_t*)node->entries;

        for( int i=0; i<node->num_entries; i++ )
        {
            for( int j=0; j<extents[2*i+1]; j++ )
            {
                table[ extents[2*i]+j ] = 1;
            }
        }
    }
}

static void debug_fs_print_table( const char* table )
{
    printf("Blocks recorded in master file table:");
    for( int i=0; i<NUM_BLOCKS; i++ )
    {
        if( i % 20 == 0 ) printf("\n%03d: ", i);
        printf("%d", table[i] );
    }
    printf("\n\n");
}

