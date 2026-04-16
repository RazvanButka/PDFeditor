char command[100];
    strcpy(command, "gcc -o ");
    strcat(command, " run ");
    strcat(command, file_name);
    strcat(command, " && ./run");
    command[strlen(command)] = '\0';
    puts(command);
    int ret = system(command);
    if (WEXITSTATUS(ret) == 0)
        send(connfd, "Succeded. Exit code 0", strlen("Succeded. Exit code 0"), 0);
    else
        send(connfd, "Failed. Exit code different than 0", strlen("Failed. Exit code different than 0"), 0);