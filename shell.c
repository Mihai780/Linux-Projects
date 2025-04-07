#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

char* execute_command(char* cmdline) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return NULL;
    }
    else if (pid == 0) {
        // Proces copil: redirectionare stdout catre capatul de scriere al pipe-ului.
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        char *args[100];
        int i = 0;
        char *token = strtok(cmdline, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;
        if (args[0] == NULL)
            exit(0);
        execvp(args[0], args);
        if (execvp(args[0], args) == -1) {
            fprintf(stdout, "%s", "1");
            exit(1);
        }
        exit(1);
    }
    else {
        // Proces parinte: citeste din pipe output-ul comenzii.
        close(pipefd[1]);
        char buffer[BUFFER_SIZE];
        int bytesRead;
        char* output = malloc(1);
        if (output == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        output[0] = '\0';
        int total_len = 0;
        
        while ((bytesRead = read(pipefd[0], buffer, BUFFER_SIZE - 1)) > 0) {
            buffer[bytesRead] = '\0';
            total_len += bytesRead;
            char* temp = realloc(output, total_len + 1);
            if (temp == NULL) {
                free(output);
                perror("realloc failed");
                exit(EXIT_FAILURE);
            }
            output = temp;
            strcat(output, buffer);
        }
        close(pipefd[0]);
        
        // Elimina caracterele de newline din output
        for (int i = 0; i < strlen(output); i++) {
            if (output[i] == '\n')
                output[i] = ' ';
        }
        if (strcmp(output,"1")==0)
        {
            output = strcpy(output,cmdline);
        }
        return output;
    }
}

char* substitute_commands(const char* input) {
    char* result = strdup(input);
    if (result == NULL) {
        perror("strdup failed");
        exit(EXIT_FAILURE);
    }

    int len = strlen(result);
    int start = -1, end = -1;
    
    // Cautam cea mai interioara pereche: retinem ultima pozitie a lui '(' si apoi prima ')' care apare
    for (int i = 0; i < len; i++) {
        if (result[i] == '(') {
            start = i;
        } else if (result[i] == ')') {
            if (start != -1) {
                end = i;
                break;
            }
        }
    }
    // Daca nu s-au gasit paranteze, se returneaza sirul curent
    if (start == -1 || end == -1) {
        return result;
    }
    
    int sub_len = end - start - 1;
    char* sub_cmd = malloc(sub_len + 1);
    if (sub_cmd == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    strncpy(sub_cmd, result + start + 1, sub_len);
    sub_cmd[sub_len] = '\0';

    char* substituted_sub = substitute_commands(sub_cmd);
    free(sub_cmd);
    char* output = execute_command(substituted_sub);
    if (output == NULL) {
        fprintf(stderr, "Comanda \"%s\" a esuat\n", substituted_sub);
        free(substituted_sub);
        free(result);
        return NULL;
    }
    free(substituted_sub);
    
    // Constructia noului sir: partea din stanga parantezei, output-ul comenzii, partea din dreapta parantezei
    int new_len = start + strlen(output) + (len - end - 1);
    char* new_result = malloc(new_len + 1);
    if (new_result == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    new_result[0] = '\0';
    strncat(new_result, result, start);
    strcat(new_result, output);
    strcat(new_result, result + end + 1);
    
    free(output);
    free(result);
    
    // Verificam daca mai sunt paranteze in noul sir
    char* final_result = substitute_commands(new_result);
    free(new_result);
    return final_result;
}

int main() {
    char* line = NULL;
    size_t len = 0;
    
    while (1) {
        //Citirea, afisarea si testarea daca se doreste iesirea din shell
        printf("> ");
        if (getline(&line, &len, stdin) == -1)
            break;
        line[strcspn(line, "\n")] = '\0';

        int nr_spatii = 0;
        while(line[nr_spatii]==' ')
        {
            nr_spatii++;
        }

        if (strncmp(line+nr_spatii, "exit",4) == 0) {
            break;
        }
        // Evaluam tot ce se intampla intre paranteze
        char* substituted_line = substitute_commands(line);
        if (substituted_line == NULL) {
            fprintf(stderr, "Eroare la procesarea substitutiei\n");
            continue;
        }
        // Se delimiteaza linia dupa spatii
        char *args[100];
        int i = 0;
        char *token = strtok(substituted_line, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;
        
        // Daca nu s-a introdus nicio comanda, continuam
        if (args[0] == NULL) {
            free(substituted_line);
            continue;
        }
        
        // Cream un proces copil pentru executarea comenzilor externe
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
        } else if (pid == 0) {
            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
        free(substituted_line);
    }
    
    free(line);
    return 0;
}