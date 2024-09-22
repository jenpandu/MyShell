#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdbool.h>

#define MAX_ARGS 64
#define MAX_PATH_LENGTH 256
#define MAX_FILES 64

void print_command_path(char *args[MAX_ARGS]);
void handle_pipes_and_redirection(char *args[MAX_ARGS]);
void handle_wildcards(char *args[MAX_ARGS]);
int match_files(const char *pattern, char *files[MAX_FILES]);

int main() {
    char *args[MAX_ARGS];
    char input[256];
    bool interactive = isatty(STDIN_FILENO);

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    int previous_command_status = 0;

    while (1) {
        if (interactive) {
            printf("mysh> ");
            fflush(stdout);
        }

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (interactive) {
                printf("Exiting my shell.\n");
            }
            break;
        }

        // Tokenize input
        int i = 0;
        args[i] = strtok(input, " \n");
        while (args[i] != NULL) {
            i++;
            args[i] = strtok(NULL, " \n");
        }
        args[i] = NULL;

        if (args[0] == NULL) {
            continue; // Empty command, prompt again
        }

        if (strcmp(args[0], "exit") == 0) {
            if (interactive) {
                printf("Exiting my shell.\n");
            }
            break;
        }

        // Handle conditional commands
        if (strcmp(args[0], "then") == 0 || strcmp(args[0], "else") == 0) {
            if (previous_command_status == 0 && strcmp(args[0], "then") == 0) {
                // Execute commands after 'then'
                while (strcmp(args[0], "else") != 0) {
                    if (fgets(input, sizeof(input), stdin) == NULL) {
                        fprintf(stderr, "Syntax error: Missing 'else'\n");
                        break;
                    }
                    int j = 0;
                    args[j] = strtok(input, " \n");
                    while (args[j] != NULL) {
                        j++;
                        args[j] = strtok(NULL, " \n");
                    }
                    args[j] = NULL;
                    handle_pipes_and_redirection(args);
                    handle_wildcards(args);
                    pid_t pid = fork();
                    if (pid < 0) {
                        perror("fork");
                    } else if (pid == 0) { // Child process
                        execvp(args[0], args);
                        perror("execvp");
                        exit(EXIT_FAILURE);
                    } else { // Parent process
                        int status;
                        waitpid(pid, &status, 0);
                        if (WIFEXITED(status)) {
                            previous_command_status = WEXITSTATUS(status);
                        }
                    }
                }
            } else if (previous_command_status != 0 && strcmp(args[0], "else") == 0) {
                // Execute commands after 'else'
                while (strcmp(args[0], "then") != 0) {
                    if (fgets(input, sizeof(input), stdin) == NULL) {
                        fprintf(stderr, "Syntax error: Missing 'then'\n");
                        break;
                    }
                    int j = 0;
                    args[j] = strtok(input, " \n");
                    while (args[j] != NULL) {
                        j++;
                        args[j] = strtok(NULL, " \n");
                    }
                    args[j] = NULL;
                    handle_pipes_and_redirection(args);
                    handle_wildcards(args);
                    pid_t pid = fork();
                    if (pid < 0) {
                        perror("fork");
                    } else if (pid == 0) { // Child process
                        execvp(args[0], args);
                        perror("execvp");
                        exit(EXIT_FAILURE);
                    } else { // Parent process
                        int status;
                        waitpid(pid, &status, 0);
                        if (WIFEXITED(status)) {
                            previous_command_status = WEXITSTATUS(status);
                        }
                    }
                }
            } else {
                fprintf(stderr, "Syntax error: Unexpected '%s'\n", args[0]);
            }
            continue;
        }

        // Handle built-in commands
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                printf("cd: Please provide a directory.\n");
            } else {
                if (chdir(args[1]) != 0) {
                    perror("chdir");
                }
            }
            continue;
        }

        if (strcmp(args[0], "pwd") == 0) {
            char cwd[256];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("getcwd");
            }
            continue;
        }

        if (strcmp(args[0], "which") == 0) {
            print_command_path(args);
            continue;
        }

        // Handle pipes, redirection, and conditional commands
        handle_pipes_and_redirection(args);

        // Handle wildcards
        handle_wildcards(args);

        // Execute command
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
        } else if (pid == 0) { // Child process
            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else { // Parent process
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                previous_command_status = WEXITSTATUS(status);
            }
        }
    }

    return 0;
}

void handle_pipes_and_redirection(char *args[MAX_ARGS]) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], "|") == 0) {
            // Handle pipe
            // Implementation skipped for brevity
            continue;
        } else if (strcmp(args[i], "<") == 0) {
            // Handle input redirection
            if (args[i + 1] != NULL) {
                int fd = open(args[i + 1], O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                args[i] = NULL; // Remove "<" token
                args[i + 1] = NULL; // Remove the filename token
            } else {
                fprintf(stderr, "Syntax error: Missing filename after '<'\n");
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(args[i], ">") == 0) {
            // Handle output redirection
            if (args[i + 1] != NULL) {
                int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[i] = NULL; // Remove ">" token
                args[i + 1] = NULL; // Remove the filename token
            } else {
                fprintf(stderr, "Syntax error: Missing filename after '>'\n");
                exit(EXIT_FAILURE);
            }
            return; // Output redirection completed, no need to continue processing
        }
        i++;
    }
}

void handle_wildcards(char *args[MAX_ARGS]) {
    int i = 0;
    while (args[i] != NULL) {
        if (strchr(args[i], '*') != NULL) {
            char *files[MAX_FILES];
            int num_files = match_files(args[i], files);
            if (num_files > 0) {
                // Replace the wildcard argument with matched files
                for (int j = 0; j < num_files; j++) {
                    args[i + j] = files[j];
                }
                args[i + num_files] = NULL; // Terminate the argument list
            }
        }
        i++;
    }
}

int match_files(const char *pattern, char *files[MAX_FILES]) {
    DIR *dir;
    struct dirent *ent;
    int num_files = 0;
    if ((dir = opendir(".")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (fnmatch(pattern, ent->d_name, 0) == 0) {
                files[num_files++] = strdup(ent->d_name);
            }
        }
        closedir(dir);
    } else {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    return num_files;
}

void print_command_path(char *args[MAX_ARGS]) {
    if (args[1] == NULL) {
        printf("which: Please provide a command.\n");
    } else {
        char *command = args[1];
        char *path = NULL;

        // Check if the command is a built-in
        if (strcmp(command, "cd") == 0 || strcmp(command, "pwd") == 0 || strcmp(command, "which") == 0 || strcmp(command, "exit") == 0) {
            printf("%s: Built-in command\n", command);
            return;
        }

        // Check if the command contains a slash (path)
        if (strchr(command, '/') != NULL) {
            path = command;
        } else {
            // Search for the command in the specified directories
            char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin"};
            int num_directories = sizeof(directories) / sizeof(directories[0]);
            for (int i = 0; i < num_directories; i++) {
                char temp_path[MAX_PATH_LENGTH];
                snprintf(temp_path, sizeof(temp_path), "%s/%s", directories[i], command);
                if (access(temp_path, X_OK) == 0) {
                    path = temp_path;
                    break;
                }
            }
        }

        if (path != NULL) {
            printf("%s\n", path);
        } else {
            printf("%s: Command not found\n", command);
        }
    }
}


