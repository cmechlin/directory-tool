#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

struct ProgramOptions {
    char *basePath;
    char *beforeDate;
    char *excludePattern;
    int verbose;
};

struct LatestFileInfo {
    char *path;
    time_t time;
};

void update_latest_file(struct LatestFileInfo *latest, const char *path, time_t ctime, time_t mtime) {
    time_t new_time = ctime > mtime ? ctime : mtime;
    if (latest->path == NULL || new_time > latest->time) {
        char *new_path = malloc(strlen(path) + 1);
        if (new_path == NULL) {
            fprintf(stderr, "Error: Cannot allocate memory\n");
            return;
        }
        strcpy(new_path, path);
        free(latest->path); // Free the previously allocated path
        latest->path = new_path;
        latest->time = new_time;
    }
}

void print_time(const time_t *time) {
    char buffer[30];
    struct tm *tm_info;

    tm_info = localtime(time);
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s\n", buffer);
}

void print_usage(const char *programName) {
    printf("Usage: %s -p <path> [-b <date>] [-e <pattern>]\n", programName);
    printf("  -p: Specify the path to search\n");
    printf("  -b: Exclude files modified before this date (format YYYY-MM-DD)\n");
    printf("  -e: Exclude files matching this pattern\n");
    printf("  -h: Display this help message\n");
}

int parse_options(int argc, char *argv[], struct ProgramOptions *options) {
    int opt;
    options->basePath = NULL;
    options->beforeDate = NULL;
    options->excludePattern = NULL;
    options->verbose = 0;

    while ((opt = getopt(argc, argv, "hp:b:e:v")) != -1) {
        switch (opt) {
            case 'h':
            case '?':
                print_usage(argv[0]);
                return 0;
            case 'p':
                options->basePath = optarg;
                break;
            case 'b':
                options->beforeDate = optarg;
                break;
            case 'e':
                options->excludePattern = optarg;
                break;
            case 'v':
                options->verbose = 1;
                break;
            default:
                fprintf(stderr, "Error: Unknown option\n");
                print_usage(argv[0]);
                return 1;
        }
    }

    if (!options->basePath) {
        fprintf(stderr, "Error: Path is required.\n");
        print_usage(argv[0]);
        return 1;
    }

    return 2; // Indicates successful parsing
}

int is_after_date(time_t file_time, const char *cutoff_date) {
    struct tm cutoff_tm = {0};
    time_t cutoff_time;

    // parse cutoff_date string (YYYY-MM-DD) to struct cutoff_time
    if (strftime(cutoff_date, "%Y-%m-%d", &cutoff_time) == NULL) {
        fprintf(stderr, "Error: Invalid date format\n");
        return 0;
    }

    cutoff_tm.tm_isdst = -1; // let mktime determine if DST is in effect
    cutoff_time = mktime(&cutoff_time);
    return difftime(file_time, cutoff_time) >= 0;
}

int is_excluded(const char *filename, const char *pattern) {
    // strstr returns NULL if pattern is not found in filename
    return strstr(filename, pattern) != NULL;
}

int recurse_dir(const char *basePath, struct LatestFileInfo *latest, struct ProgramOptions *options) {
    DIR *dir;
    struct dirent *dp;
    struct stat statbuf;
    char path[1024];
    
    if((dir = opendir(basePath)) == NULL) {
        fprintf(stderr,"Error: Cannot open %s\n", basePath);
        return 1;
    }

    while ((dp = readdir(dir)) != NULL) {
        // if the directory is . or .., skip it
        if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        // construct the full path of the directory
        snprintf(path, sizeof(path), "%s\\%s", basePath, dp->d_name);

        // get the file information
        if(stat(path, &statbuf) == -1) {
            fprintf(stderr, "Error: Cannot get the file information of %s\n", path);
            continue;
        }

        // if the file is older than the cutoff date, skip it
        if(options->beforeDate != NULL && is_after_date(statbuf.st_mtime, options->beforeDate)) {
            continue;
        }

        // if the file is excluded, skip it
        if(options->excludePattern != NULL && is_excluded(dp->d_name, options->excludePattern)) {
            continue;
        }

        update_latest_file(latest, path, statbuf.st_ctime, statbuf.st_mtime);

        // if the file is a directory, recurse it
        if(S_ISDIR(statbuf.st_mode)) {
            if(options->verbose) printf("Searching %s\\\n", path);            
            recurse_dir(path, latest, options);
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    struct ProgramOptions options;
    struct LatestFileInfo latest = {NULL, 0};

    int result = parse_options(argc, argv, &options);
    if(result != 2){
        return result = 0 ? 0 : 1;
    }

    // go through the directory and find the latest file
    recurse_dir(options.basePath, &latest, &options);

    if(latest.path != NULL && latest.time != 0) {
        printf("File: %s Date: ", latest.path);
        print_time(&latest.time);
    } else {
        printf("No file found\n");
    }

    if (latest.path != NULL) {
        free(latest.path); // Free the memory allocated to latest.path
    }
    
    return 0;
}