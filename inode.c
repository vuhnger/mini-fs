#include "inode.h"
#include "block_allocation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int MAX_ID = 0;

struct inode* create_file( struct inode* parent, const char* name, char readonly, int size_in_bytes )
{
    fprintf( stderr, "%s is not implemented\n", __FUNCTION__ );
    return NULL;
}

struct inode* create_dir( struct inode* parent, const char* name )
{
    fprintf( stderr, "%s is not implemented\n", __FUNCTION__ );
    return NULL;
}

struct inode* find_inode_by_name( struct inode* parent, const char* name )
{
    fprintf( stderr, "%s is not implemented\n", __FUNCTION__ );
    return NULL;
}

int delete_file( struct inode* parent, struct inode* node )
{
    fprintf( stderr, "%s is not implemented\n", __FUNCTION__ );
    return -1;
}

int delete_dir( struct inode* parent, struct inode* node )
{
    fprintf( stderr, "%s is not implemented\n", __FUNCTION__ );
    return -1;
}

void save_inodes( const char* master_file_table, struct inode* root )
{
    fprintf( stderr, "%s is not implemented\n", __FUNCTION__ );
    return;
}

struct inode *load_inodes(const char *master_file_table) {
    FILE *file = fopen(master_file_table, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", master_file_table);
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
        fread(entries, sizeof(uintptr_t), num_entries, file);

        struct inode *node = malloc(sizeof(struct inode));
        node->id = id;
        node->name = name;
        node->is_directory = is_directory;
        node->is_readonly = is_readonly;
        node->filesize = filesize;
        node->num_entries = num_entries;
        node->entries = entries;

        if (id >= inode_count) {
            inode_map = realloc(inode_map, (id + 1) * sizeof(struct inode *));
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

/* This static variable is used to change the indentation while debug_fs
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

