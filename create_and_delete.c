#include "inode.h"
#include "block_allocation.h"

#include <stdio.h>

void hexdump(const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Feil ved åpning av fil for hexdump");
        return;
    }

    printf("\n=== Hexdump av %s ===\n", filename);
    unsigned char buffer[16];  // Leser 16 bytes av gangen
    size_t bytesRead;
    size_t offset = 0; // For å vise posisjon i filen

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        printf("%08zx  ", offset); // Utskrift av offset i hex

        // Skriv ut hex-verdier
        for (size_t i = 0; i < bytesRead; i++) {
            printf("%02x ", buffer[i]);
            if (i == 7) printf(" "); // Ekstra mellomrom midt i linjen
        }

        // Fyll inn mellomrom hvis det er færre enn 16 bytes lest
        for (size_t i = bytesRead; i < 16; i++) {
            printf("   ");
            if (i == 7) printf(" ");
        }

        // Skriv ut ASCII-tegnene ved siden av
        printf(" |");
        for (size_t i = 0; i < bytesRead; i++) {
            printf("%c", (buffer[i] >= 32 && buffer[i] <= 126) ? buffer[i] : '.');
        }
        printf("|\n");

        offset += bytesRead;
    }

    fclose(file);
}

int main( int argc, char* argv[] )
{
    if( argc != 3 )
    {
        fprintf( stderr, "Usage: %s MFT BAT\n"
                         "       where\n"
                         "       MFT is the name of the master_file_table\n"
                         "       BAT is the name of the block allocation table\n"
                         , argv[0] );
        exit( -1 );
    }

    char* mft_name = argv[1];
    char* bat_name = argv[2];

    set_block_allocation_table_name( bat_name );

    /* format_disk() makes sure that the simulated
     * disk is empty. It creates a file named
     * block_allocation_table that contains only
     * zeros. */
    format_disk();

    /* debug_disk() write the current content of the
     * block_allocation_table that simulates whether
     * blocks on disk contain file data (1) or not (0).
     */
    debug_disk();

    printf("===================================\n");
    printf("= Create a whole filesystem       =\n");
    printf("===================================\n");
    struct inode* root     = create_dir( NULL, "/" );
    struct inode* f_kern    = create_file( root, "kernel", 1, 20000 );
    struct inode* dir_etc   = create_dir( root, "etc" );
    struct inode* f_hosts   = create_file( dir_etc, "hosts", 0, 200 );
    struct inode* dir_usr   = create_dir( root, "usr" );
    struct inode* dir_bin   = create_dir( dir_usr, "bin" );
    struct inode* dir_local = create_dir( dir_usr, "local" );
    struct inode* dir_lbin  = create_dir( dir_local, "bin" );
    struct inode* f_nvcc    = create_file( dir_lbin, "nvcc", 0, 28000 );
    struct inode* f_gcc     = create_file( dir_lbin, "gcc", 1, 12623 );
    struct inode* dir_home  = create_dir( root, "home" );
    struct inode* d_in2140  = create_dir( dir_home, "in2140" );
    struct inode* d_obligz  = create_file( d_in2140, "oblig.tgz", 0, 15000 );
    struct inode* d_oblig   = create_dir( d_in2140, "oblig" );
    struct inode* f_cmake   = create_file( d_oblig, "CMakeLists.txt", 0, 5486 );
    struct inode* f_inodec  = create_file( d_oblig, "inode.c", 0, 16988 );
    struct inode* f_inodeh  = create_file( d_oblig, "inode.h", 0, 4152 );
    struct inode* f_ls      = create_file( dir_bin, "ls", 1, 14322 );
    struct inode* f_ps      = create_file( dir_bin, "ps", 1, 13800 );

    debug_fs( root );
    debug_disk();

    printf("===================================\n");
    printf("= Deleting some things            =\n");
    printf("===================================\n");
    int success;

    printf("Trying to delete file gcc from / (should fail)\n");
    success = delete_file( f_gcc, root );
    printf("Deletion %s\n", success ? "succeeded" : "failed");

    printf("Trying to delete file oblig.tgz from /home/in2140 (should succeed)\n");
    success = delete_file( d_in2140, d_obligz );
    printf("Deletion %s\n", success ? "succeeded" : "failed");

    printf("Trying to delete file nvcc from /usr/local/bin (should succeed)\n");
    success = delete_file( dir_lbin, f_nvcc );
    printf("Deletion %s\n", success ? "succeeded" : "failed");

    printf("Trying to delete directory etc from / (should fail)\n");
    success = delete_dir( dir_etc, root );
    printf("Deletion %s\n", success ? "succeeded" : "failed");

    printf("Trying to delete file hosts from /etc (should succeed)\n");
    success = delete_file( f_hosts, dir_etc );
    printf("Deletion %s\n", success ? "succeeded" : "failed");

    printf("Trying to delete directory etc from / (should succeed)\n");
    success = delete_dir( dir_etc, root );
    printf("Deletion %s\n", success ? "succeeded" : "failed");

    debug_fs( root );
    debug_disk();

    hexdump(mft_name);
    save_inodes( mft_name, root );
    hexdump(mft_name);

    fs_shutdown( root );

    printf( "++++++++++++++++++++++++++++++++++++++++++++++++\n" );
    printf( "+ All inodes structures have been\n" );
    printf( "+ deleted. The inode info is stored in\n" );
    printf( "+ %s\n", mft_name );
    printf( "+ The allocated file blocks are stored in\n" );
    printf( "+ %s\n", bat_name );
    printf( "++++++++++++++++++++++++++++++++++++++++++++++++\n" );
}

