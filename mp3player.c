#include <stdio.h>
#include <stdint.h> 
#include <stdlib.h>
#include <string.h>
#include <mpg123.h>
#include <ao/ao.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h> // For seeding the random number generator

#define BUFFER_SIZE 1024
#define ARRAY_SIZE 256
// mp3player.c

// Function to check if a path is a directory
int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

// Function to shuffle an array of strings
void shuffle(char *array[], int n) {
    if (n > 1) {
        size_t i;
        for (i = 0; i < n - 1; i++) {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          char *t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}
// Function to play MP3 files
void play_mp3_files(char *file_paths[], int count, int shuffle_files) {
    mpg123_handle *mp3_handle = mpg123_new(NULL, NULL);
    if (mp3_handle == NULL) {
        fprintf(stderr, "Unable to create mpg123 handle.\n");
        return;
    }
    if (shuffle_files) {
        shuffle(file_paths, count);
    }
    for (int i = 0; i < count; ++i) {
        printf("Playing %s\n", file_paths[i]);
        // Open the MP3 file
        if (mpg123_open(mp3_handle, file_paths[i]) != MPG123_OK) {
            fprintf(stderr, "Error opening MP3 file %s.\n", file_paths[i]);
            continue; // Skip to the next file
        }

        // Retrieve the MP3 file's audio format
        long rate;
        int channels, encoding;
        mpg123_getformat(mp3_handle, &rate, &channels, &encoding);
        ao_sample_format format = {0};
        format.bits = mpg123_encsize(encoding) * 8;
        format.rate = rate;
        format.channels = channels;
        format.byte_format = AO_FMT_NATIVE;

        // Open the default audio device
        int driver = ao_default_driver_id();
        ao_device *device = ao_open_live(driver, &format, NULL);
        if (device == NULL) {
            fprintf(stderr, "Error opening audio device.\n");
            mpg123_close(mp3_handle);
            continue; // Skip to the next file
        }

        // Decode and play the MP3 buffer by buffer
        unsigned char *buffer;
        size_t buffer_size = mpg123_outblock(mp3_handle);
        size_t done;
        buffer = (unsigned char *)malloc(buffer_size * sizeof(unsigned char));

        int err;
        while ((err = mpg123_read(mp3_handle, buffer, buffer_size, &done)) == MPG123_OK) {
            if (ao_play(device, (char *)buffer, done) == 0) {
                fprintf(stderr, "Error playing audio.\n");
                break;
            }
        }

        if (err != MPG123_DONE) {
            fprintf(stderr, "Error reading MP3 file %s: %s\n", file_paths[i], mpg123_strerror(mp3_handle));
        }

        // Free the buffer and close the audio device
        free(buffer);
        ao_close(device);
        mpg123_close(mp3_handle);
    }
    mpg123_delete(mp3_handle);
}

int main() {
  
    // Initialize the mpg123 library and check for errors
    if (mpg123_init() != MPG123_OK) {
        fprintf(stderr, "Unable to initialize mpg123 library.\n");
        return EXIT_FAILURE;
    }
    // Initialize the libao library 
    ao_initialize();

    // Seed the random number generator for the shuffling
    srand((unsigned int)time(NULL));

    // Prompt for the MP3 file path
    char path[BUFFER_SIZE];
    printf("Enter the path to the MP3 file or directory: ");
    
    fgets(path, sizeof(path), stdin);
    size_t len = strlen(path);
    if (len > 0 && path[len - 1] == '\n') {
        path[len - 1] = '\0';
    }
    char *file_paths[ARRAY_SIZE];
    int file_count = 0;

    if (is_directory(path)) {
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(path)) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                if (strstr(ent->d_name, ".mp3")) {
                    // Allocate memory for the full file path
                    char *file_path = malloc(BUFFER_SIZE);
                    snprintf(file_path, BUFFER_SIZE, "%s/%s", path, ent->d_name);
                    file_paths[file_count++] = file_path;

                    if (file_count >= ARRAY_SIZE) {
                        fprintf(stderr, "Too many MP3 files. Some files may not be played.\n");
                        break;
                    }
                }
            }
            closedir(dir);
        } else {
            perror("Could not open directory");
            return EXIT_FAILURE;
        }
        if (file_count > 0) {
            char shuffle_input;
            int shuffle_files = 0; // Default no shuffle
            printf("Shuffle files? (y/n): ");
            scanf(" %c", &shuffle_input); // 
            // Convert the character input to an integer flag
            shuffle_files = (shuffle_input == 'y' || shuffle_input == 'Y');

            // Play the MP3 files
            play_mp3_files(file_paths, file_count, shuffle_files);

            // Free allocated memory for file paths
            for (int i = 0; i < file_count; ++i) {
                free(file_paths[i]);
            }
        } else {
            printf("No MP3 files found in the directory.\n");
        }
    }
    else {
        // Assume the input is a single MP3 file path
        char *file_path = malloc(BUFFER_SIZE);
        snprintf(file_path, BUFFER_SIZE, "%s", path);
        file_paths[file_count++] = file_path;

        // Play the single MP3 file
        play_mp3_files(file_paths, file_count, 0); 
    }
    for (int i = 0; i < file_count; ++i) {
        free(file_paths[i]);
    }
    mpg123_exit();
    ao_shutdown();
    return EXIT_SUCCESS;
}