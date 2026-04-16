#include <pthread.h>

#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <dirent.h>
#include <sys/stat.h>
#include "operationsSOAP.nsmap"
#include "soapH.h"

/*
extern WINDOW * mainwnd ;
extern pthread_mutex_t curmtx ;
WINDOW *soapwnd;
*/

/* Reuse this function. Previously from inetds.c */

long create_client_id () ;
void closewin (WINDOW *wnd) ;

char * runfile(char * program, char* filename, char * type){
    int ofd1 = dup(STDOUT_FILENO), ofd2 = dup(STDERR_FILENO);
    int fd = open(filename, O_RDWR | O_CREAT, O_TRUNC), nfd = open("temp.txt", O_RDWR | O_CREAT), nfd2 = dup(nfd);
    system("chmod u+rwx ./serv_files/SOAP/*");
    fprintf(stderr, "filename:%s\n",filename);
    write(fd, program, strlen(program));//salveaza continutul pe server
    char command[500]="";
    switch(fork()){
        case 0: // child
        dup2(nfd, STDOUT_FILENO);
        dup2(nfd2, STDERR_FILENO);
        if(strcmp(type, ".c") == 0){
            strcpy(command, "gcc -o ");
            strcat(command, "runnable ");
            strcat(command, filename);
            strcat(command, " && ./runnable");
        }
        else if(strcmp(type, ".cpp") == 0){
            strcpy(command, "g++ -o ");
            strcat(command, "runnable ");
            strcat(command, filename);
            strcat(command, " && ./runnable");
        }
        else if(strcmp(type, ".py") == 0){
            strcpy(command, "python3 ");
            strcat(command, filename);
        }
        else if(strcmp(type, ".java") == 0){
            strcpy(command, "javac ");
            strcat(command, filename);
            strcat(command, " && java ");
            strcat(command, filename);
        }
        command[strlen(command)] = 0;
        close(nfd);
        close(nfd2);
        system(command); // compileaza codul
        break;
        default: // parent
        wait(NULL);
        break;
    }
    dup2(ofd1, STDOUT_FILENO);
    dup2(ofd2, STDERR_FILENO);
    close(ofd1);
    close(ofd2);
    char * res = malloc(1000);
    strcpy(res, "");
    system("chmod u+rwx temp.txt");
    nfd = open("temp.txt", O_RDWR);
    //read(nfd, res, 1000);
   // buff = (char *)malloc(sizeof(char) * SIZE);
   char * temp = malloc(256);
   int bytesRead;
    while ((bytesRead = read(nfd, temp, 256)) > 0)
    {
        temp[bytesRead] = '\0';
        strcat(res, temp);
    }
    // unlink("temp.txt");
    return res;
}

char * getFilename(char * id){
    int i;
    char * filename = malloc(200);
    strcpy(filename, "./serv_files/SOAP/");
    int ss = strlen(filename);
    for(i=1;i<strlen(id)-1;i++)
        filename[ss++]=id[i];
    filename[ss] = 0;
    return filename;
}

void *soap_main (void *args) {
    struct soap soap ;
    int msd, csd, port, reuseAddrON = 1 ;
    port = *(int *)(args);
    soap_init (&soap) ;

    soap.bind_flags=SO_REUSEADDR ;
    msd = soap_bind (&soap, "127.0.0.1", port, 100) ;
    if (!soap_valid_socket(msd)) {
        soap_print_fault (&soap, stderr) ;
        pthread_exit (NULL) ;
    } else {
        setsockopt(msd, SOL_SOCKET, SO_REUSEADDR, &reuseAddrON, sizeof(reuseAddrON)) ;
    }

    for (;;) {
        csd = soap_accept (&soap) ;
        if (csd < 0) { soap_print_fault (&soap, stderr); break ; }
//    soapwnd = newwin (16, 70, 5, 5) ;
        if (soap_serve (&soap) != SOAP_OK) soap_print_fault (&soap, stderr) ;
        soap_destroy (&soap) ;
        soap_end (&soap) ;
    }
    soap_done (&soap) ;
    pthread_exit (NULL) ;
}
/* */










// operatia 1: trimite fisier spre executie catre serfver


int __ns1__storeFile(
        struct soap * s, // soap
        struct _ns1__storeFile*             ns1__storeFile,	///< Input parameter
        struct _ns1__storeFileResponse     *ns1__storeFileResponse	///< Output parameter
){
   // ns1__storeFileResponse->out = "ceau"; // raspunsul
    struct soap_multipart *attachment;
  //  printf("Attachements\n");
    int n = 0;
    char *res;
    soap_set_mime(s, NULL, NULL); //enable attachments
    char * fname;
    for (attachment = s->mime.list; attachment; attachment = attachment->next) {
        ++n;
        if (n == 2) {
            /*  if (strstr(attachment->id, ".c") || strstr(attachment->id, ".py") || strstr(attachment->id, ".cpp") ||
                  strstr(attachment->id, ".java")) { */
            printf("Part %d:\n", n);
            printf("ptr        =%p\n", attachment->ptr);
            printf("Content:\n%s\n", attachment->ptr);
            // write(1, attachment->ptr, sizeof(attachment->ptr));
            //    write(1, "\n", 1);
            printf("size       =%ul\n", attachment->size);
            printf("id         =%s\n", attachment->id ? attachment->id : "");
            printf("type       =%s\n", attachment->type ? attachment->type : "");
            printf("location   =%s\n", attachment->location ? attachment->location : "");
            printf("description=%s\n", attachment->description ? attachment->description : "");
            char *type = malloc("10");
            if (strstr(attachment->type, ".c"))
                strcpy(type, ".c");
            else if (strstr(attachment->type, ".cpp"))
                strcpy(type, ".cpp");
            else if (strstr(attachment->type, ".java"))
                strcpy(type, ".java");
            else if (strstr(attachment->type, ".py"))
                strcpy(type, ".py");
            //  fprintf(stderr, "Type=%s\n", type);
            /// proceseaza fisier
//////
            res = runfile(attachment->ptr, getFilename(attachment->id), type);
            fname = getFilename(attachment->id);
            //char * log_content = getLogContent(attachment->ptr); // creeaza log-ul
            soap_set_mime_attachment(s, res, strlen(res), //adauga atasament la raspuns
                                     SOAP_MIME_NONE, "text/plain",
                                     NULL, // id, poate fi omis
                                     "fisier.txt", // numele/calea fisierului
                                     "descriere" // descriere
            );

            //}
        }
    }
    char * resp = malloc(200);
    sprintf(resp, ":: server :: am executat fisierul %s", fname);
    ns1__storeFileResponse->out = resp;
   // sprintf(ns1__storeFileResponse->out, "Salut %s", ns1__storeFile->in);
    return SOAP_OK ;
}


// operatia 2: creeaza fisier pe server

int __ns1__createFile(
        struct soap * s, // soap
        struct _ns1__createFile*             ns1__createFile,	///< Input parameter
        struct _ns1__createFileResponse     *ns1__createFileResponse	///< Output parameter
){
    int fd;
    char path[200];
    sprintf(path, "./serv_files/SOAP/%s", ns1__createFile->in);
    path[strlen(path)] = 0;
    fd = open(path, O_RDWR | O_CREAT);
    if(fd < 0){
        soap_print_fault(s, stderr);
        return SOAP_FAULT;
    }
    system("chmod u+rwx ./serv_files/SOAP/*");
    char * resp = malloc(100);
    sprintf(resp, ":: fisier creeat pe server :: %s",ns1__createFile->in);
    ns1__createFileResponse->out = resp;
    return SOAP_OK;
}


int __ns1__clientLogFiles(
        struct soap * s,
                struct _ns1__clientLogFiles* ns1__clientLogFiles,
                struct _ns1__clientLogFilesResponse1* ns1__clientLogFilesResponse
        ){

    int fd = open("temp.txt", O_RDWR | O_CREAT), fdorig;
    soap_set_mime(s, NULL, NULL); //enable attachments
    switch(fork()){
        case 0: // child
            dup2(fd, STDOUT_FILENO);
            system("ls -lia ./client_files/INET");
            system("ls -lia ./client_files/SOAP");
            break;
        default:
            wait(NULL);
            break;
    }
    char buf[100000];
    close(fd);
    system("chmod u+rwx temp.txt");
    fdorig = open("temp.txt", O_RDWR);
    read(fdorig, buf, 100000);
    soap_set_mime_attachment(s, buf, strlen(buf), //adauga atasament la raspuns
                             SOAP_MIME_NONE, "text/plain",
                             NULL, // id, poate fi omis
                             "client_log_files.txt", // numele/calea fisierului
                             "log-urile clientilor" // descriere
    );
    ns1__clientLogFilesResponse->out = malloc(200);
    sprintf(ns1__clientLogFilesResponse->out,
            ":: server :: Informatiile legate de log-urile de la nivel de client sunt disponibile mai jos");
    unlink("temp.txt");
    return SOAP_OK;
}

int __ns1__filesInCertainProgLanguage(
        struct soap * s,
                struct _ns1__filesInCertainProgLanguage* req,
                struct _ns1__filesInCertainProgLanguageResponse1 * rsp
        ){
    char command[200];
   /* if(strcmp(req->in, "py") && strcmp(req->in, "java") && strcmp(req->in, "c") && strcmp(req->in, "cpp")){
        soap_print_fault(s, stderr);
        rsp->out = "Invalid extension!";
        return SOAP_FAULT;
    } */
    int fd = open("temp.txt", O_RDWR | O_CREAT), fdorig;
    //soap_set_mime(s, NULL, NULL); //enable attachments
    switch(fork()){
        case 0: // child
            dup2(fd, STDOUT_FILENO);
            sprintf(command, "ls ./serv_files/INET/*.%s && ls ./serv_files/SOAP/*.%s", req->in, req->in);
            system(command);
            break;
        default:
            wait(NULL);
            break;
    }
    char buf[100000];
    close(fd);
    system("chmod u+rwx temp.txt");
    fdorig = open("temp.txt", O_RDWR);
    read(fdorig, buf, 100000);
    char description[200];
    sprintf(description, "fisierele de la nivel de server scrise in limbajul de programare .%s", req->in);

    rsp->out = malloc(100000);
    sprintf(rsp->out, "\t%s\n%s\n", description, buf);
    unlink("temp.txt");
    return SOAP_OK;
}

int __ns1__filesStatistics(
        struct soap *s,
                struct _ns1__filesStatistics * rq,
                struct _ns1__filesStatisticsResponse1 * rsp
        ){
    /*echo "{'py':$(ls -l ./serv_files/INET/*.py|wc -l), 'c':$(ls -l ./serv_files/INET/*.c|wc -l), 'cpp': $(ls -l ./serv_files/INET/*.cpp|wc -l), 'java': $(ls -l ./serv_files/INET/*.java|wc -l 2>/dev/null)}"*/
    int fd = open("temp.txt", O_RDWR | O_CREAT), fdorig;
    soap_set_mime(s, NULL, NULL); //enable attachments
    switch(fork()){
        case 0: // child
            dup2(fd, STDOUT_FILENO);
         //   system("ls -lia ./client_files/INET");
        //    system("ls -lia ./client_files/SOAP");
        system("echo \"{\n\t'py':$(ls -l ./serv_files/SOAP/*.py|wc -l),\n\t 'c':$(ls -l ./serv_files/SOAP/*.c|wc -l),\n\t 'cpp': $(ls -l ./serv_files/SOAP/*.cpp|wc -l),\n\t 'java': $(ls -l ./serv_files/SOAP/*.java|wc -l 2>/dev/null)\n}\"");
            break;
        default:
            wait(NULL);
            break;
    }
    char buf[100000];
    close(fd);
    system("chmod u+rwx temp.txt");
    fdorig = open("temp.txt", O_RDWR);
    read(fdorig, buf, 100000);
    rsp->out = malloc(buf);
    strcpy(rsp->out, buf);
    soap_set_mime_attachment(s, buf, strlen(buf), //adauga atasament la raspuns
                             SOAP_MIME_NONE, "text/plain",
                             NULL, // id, poate fi omis
                             "statistics.json", // numele/calea fisierului
                             "statistici privind numarul programelor scrise in limbaje de programare" // descriere
    );
    unlink("temp.txt");
    return SOAP_OK;
}

int __ns1__largestServFile(
        struct soap *s,
                struct _ns1__largestServFile * rq,
                struct _ns1__largestServFileResponse1 * rsp
        ){
    // informatii legate de cel mai mare fisier, aici avem transfer de fisiere
    // optiunea e trimisa deja, caut cel mai mare fisier
    DIR * dptr = opendir("./serv_files/SOAP");
    struct dirent * sdir;
    char str[200];
    struct stat sb;
    pid_t p;
    char command[500];
    int fd;
    int maxsize = 0;
    char maxfile[200];
    while((sdir = readdir(dptr)) != NULL){
        if(sdir->d_type == 4){
            if(sdir->d_name[0] == '.'){
                // . sau ..
                stat(sdir->d_name,&sb);
                //size=buf.st_size;
                //  pf("size=%d\n",size);
            }
        }
        else
        {
            strcpy(str,"./serv_files/SOAP");
            strcat(str,"/");
            strcat(str,sdir->d_name);
            //    fprintf(stderr, "%s\n", str);
            stat(str,&sb); // dimensiunea fisierului
            if(sb.st_size > maxsize) {
                maxsize = sb.st_size;
                strcpy(maxfile, str); // ii salvez si calea
            }
        }
    }
    // acum afisez informatiile legate de fisier
    switch(p = fork()){
        case 0: // child
            creat("temp.txt", O_RDWR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); // creeaza fisierul
            fd = open("temp.txt", O_RDWR);
            dup2(fd, STDOUT_FILENO);
            // close(fd); //ca sa mearga dup2 in system
            fprintf(stdout, "\t\t[Informatii generale legate de fisierul %s]\n", maxfile);
            fprintf(stdout, "\t\t");
            for(int i=0;i<sizeof("[Informatii generale legate de fisierul ") + sizeof(maxfile) + 1; i++)
                fprintf(stdout, "=");
            fprintf(stdout,"\n");
            strcpy(command, "ls -lia ");
            strcat(command,maxfile);
            system(command); // afiseaza informatii generale
            fprintf(stdout,"\n");
            fprintf(stdout, "\t\t[Informatii legate de continuturile din iNOD pentru fisierul %s]\n", maxfile);
            fprintf(stdout, "\t\t");
            for(int i=0;i<sizeof("[Informatii legate de continuturile din iNOD pentru fisierul ") + sizeof(maxfile) + 1; i++)
                fprintf(stdout, "=");
            fprintf(stdout,"\n");
            strcpy(command, "stat -s ");
            strcat(command,maxfile);
            system(command); // afiseaza informatii legate de continuturile din INOD
            fprintf(stdout,"\n");
            fprintf(stdout, "\t\t[Continutul fisierului %s]\n", maxfile);
            fprintf(stdout, "\t\t");
            for(int i=0;i<sizeof("[Continutul fisierului ") + sizeof(maxfile) + 1; i++)
                fprintf(stdout, "=");
            fprintf(stdout,"\n");
            strcpy(command, "cat -b ");
            strcat(command, maxfile);
            system(command); // afiseaza-i continutul
            fprintf(stdout,"\n");
        default:
            wait(NULL);
            break;
    }
    close(fd); // inchide fisierul
    // acum citeste din fisier si trimite inapoi la client
    fd = open("temp.txt", O_RDWR);
    char buf[100000];
    read(fd, buf, 100000);
   /* soap_set_mime(s, NULL, NULL); //enable attachments
    soap_set_mime_attachment(s, buf, strlen(buf), //adauga atasament la raspuns
                             SOAP_MIME_NONE, "text/plain",
                             NULL, // id, poate fi omis
                             "largest_SOAP_serv_file.txt", // numele/calea fisierului
                             "informatii legate de cel mai mare fisier de pe server-ul SOAP" // descriere
    ); */
    rsp->out = malloc(1000000);
    sprintf(rsp->out,
            "Informatiile atasate legate de fisierul %s sunt disponibile mai jos\n%s\n",
    maxfile, buf);
    unlink("temp.txt");  // sterge fisierul temporar, nu mai avem nevoie de el

    return SOAP_OK;
}

int __ns1__smallestServFile(
        struct soap * s,
                struct _ns1__smallestServFile * rq,
                        struct _ns1__smallestServFileResponse1 * rsp
        ){
    // informatii legate de cel mai mare fisier, aici avem transfer de fisiere
    // optiunea e trimisa deja, caut cel mai mare fisier
    DIR * dptr = opendir("./serv_files/SOAP");
    struct dirent * sdir;
    char str[200];
    struct stat sb;
    pid_t p;
    char command[500];
    int fd;
    int maxsize = 1000000;
    char maxfile[200];
    while((sdir = readdir(dptr)) != NULL){
        if(sdir->d_type == 4){
            if(sdir->d_name[0] == '.'){
                // . sau ..
                stat(sdir->d_name,&sb);
                //size=buf.st_size;
                //  pf("size=%d\n",size);
            }
        }
        else
        {
            strcpy(str,"./serv_files/SOAP");
            strcat(str,"/");
            strcat(str,sdir->d_name);
            //    fprintf(stderr, "%s\n", str);
            stat(str,&sb); // dimensiunea fisierului
            if(sb.st_size < maxsize && sb.st_size > 0) {
                maxsize = sb.st_size;
                strcpy(maxfile, str); // ii salvez si calea
            }
        }
    }
    // acum afisez informatiile legate de fisier
    switch(p = fork()){
        case 0: // child
            creat("temp.txt", O_RDWR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); // creeaza fisierul
            fd = open("temp.txt", O_RDWR);
            dup2(fd, STDOUT_FILENO);
            // close(fd); //ca sa mearga dup2 in system
            fprintf(stdout, "\t\t[Informatii generale legate de fisierul %s]\n", maxfile);
            fprintf(stdout, "\t\t");
            for(int i=0;i<sizeof("[Informatii generale legate de fisierul ") + sizeof(maxfile) + 1; i++)
                fprintf(stdout, "=");
            fprintf(stdout,"\n");
            strcpy(command, "ls -lia ");
            strcat(command,maxfile);
            system(command); // afiseaza informatii generale
            fprintf(stdout,"\n");
            fprintf(stdout, "\t\t[Informatii legate de continuturile din iNOD pentru fisierul %s]\n", maxfile);
            fprintf(stdout, "\t\t");
            for(int i=0;i<sizeof("[Informatii legate de continuturile din iNOD pentru fisierul ") + sizeof(maxfile) + 1; i++)
                fprintf(stdout, "=");
            fprintf(stdout,"\n");
            strcpy(command, "stat -s ");
            strcat(command,maxfile);
            system(command); // afiseaza informatii legate de continuturile din INOD
            fprintf(stdout,"\n");
            fprintf(stdout, "\t\t[Continutul fisierului %s]\n", maxfile);
            fprintf(stdout, "\t\t");
            for(int i=0;i<sizeof("[Continutul fisierului ") + sizeof(maxfile) + 1; i++)
                fprintf(stdout, "=");
            fprintf(stdout,"\n");
            strcpy(command, "cat -b ");
            strcat(command, maxfile);
            system(command); // afiseaza-i continutul
            fprintf(stdout,"\n");
        default:
            wait(NULL);
            break;
    }
    close(fd); // inchide fisierul
    // acum citeste din fisier si trimite inapoi la client
    fd = open("temp.txt", O_RDWR);
    char buf[100000];
    read(fd, buf, 100000);
    /*
    soap_set_mime(s, NULL, NULL); //enable attachments
    soap_set_mime_attachment(s, buf, strlen(buf), //adauga atasament la raspuns
                             SOAP_MIME_NONE, "text/plain",
                             NULL, // id, poate fi omis
                             "smallest_SOAP_serv_file.txt", // numele/calea fisierului
                             "informatii legate de cel mai mic fisier de pe server-ul SOAP" // descriere
    ); */
    rsp->out = malloc(1000000);
    sprintf(rsp->out,
            "Informatiile atasate legate de fisierul %s sunt disponibile mai jos\n%s",
            maxfile, buf);
    unlink("temp.txt");  // sterge fisierul temporar, nu mai avem nevoie de el

    return SOAP_OK;
}

int __ns1__totalServFilesSize(
        struct soap * s,
                struct _ns1__totalServFilesSize * rq,
                        struct _ns1__totalServFilesSizeResponse1 * rsp
        ){
    DIR * dptr;
    struct dirent * sdir;
    struct stat sb;
    char str[500];
    long size = 0;
    dptr = opendir("./serv_files/SOAP");
    while((sdir = readdir(dptr)) != NULL){
        if(sdir->d_type == 4){
            if(sdir->d_name[0] == '.'){
                // . sau ..
                stat(sdir->d_name,&sb);
            }
        }
        else
        {
            strcpy(str,"./serv_files/INET");
            strcat(str,"/");
            strcat(str,sdir->d_name);
            fprintf(stderr, "%s\n", str);
            stat(str,&sb);
            size+=sb.st_size;
        }
    }
    double res = 9.31 * pow(10, -9) * size;
    char * result = malloc(130);
    sprintf(result, "Total fisiere de pe server SOAP: %.15lf GB", res);
   // fprintf(stderr, "Fisierele de pe server-ul SOAP ocupa in total %s GB\n", result);
    result[strlen(result)] = 0;
    rsp->out = result;
    return SOAP_OK;
}

int __ns1__modifyFile(
        struct soap * s,
                struct _ns1__modifyFile *rq,
                        struct _ns1__modifyFileResponse * rsp
        ){

    int fd;
    char path[200];
    char * p = strtok(rq->in, " ");
    sprintf(path, "./serv_files/SOAP/%s", p);
    char *p2 = strtok(NULL, " ");
    char newpath[200];
    sprintf(newpath, "./serv_files/SOAP/%s", p2);
    path[strlen(path)] = 0;
    newpath[strlen(newpath)] = 0;
    char command[500];
    sprintf(command, "mv %s %s", path, newpath);
    command[strlen(command)] = 0;
    system(command);
    system("chmod u+rwx ./serv_files/SOAP/*");
    char * resp = malloc(100);
    sprintf(resp, ":: fisier %s modificat pe server :: %s",p, p2);
    rsp->out = resp;
    return SOAP_OK;
}

int __ns1__removeFile(
        struct soap * s,
                struct _ns1__removeFile * rq,
                        struct _ns1__removeFileResponse * rsp
        ){
    int fd;
    char path[200];
    sprintf(path, "./serv_files/SOAP/%s", rq->in);
    path[strlen(path)] = 0;
   /* if(unlink(path) == -1){
        rsp->out = malloc(200);
        sprintf(rsp->out, "Fisierul indicat %s nu exista la nivel de server!", rq->in);
       // soap_print_fault(s, stderr);
        return SOAP_FAULT;
    }*/
   unlink(path);
    system("chmod u+rwx ./serv_files/SOAP/*");
    char * resp = malloc(400);
    sprintf(resp, ":: fisier sters pe server :: %s",rq->in);
    rsp->out = resp;
    return SOAP_OK;
}

int __ns1__servFiles(
        struct soap * s,
                struct _ns1__servFiles * rq,
                 struct _ns1__servFilesResponse1 * rsp
        ){
    int fd, fdorig;
    fd = open("temp.txt", O_RDWR | O_CREAT);
    switch(fork()){
        case 0:
            dup2(fd, STDOUT_FILENO);
            system("ls -lia ./serv_files/SOAP");
            break;
        default:
            wait(NULL);
            break;
    }
    close(fd);
    system("chmod u+rwx temp.txt");
    fdorig = open("temp.txt", O_RDWR);
    char buf[100000];
    read(fdorig, buf, 100000);
    soap_set_mime(s, "SOAP-ENV:BODY", "SOAP-ENV:BODY"); //enable attachments
    soap_set_mime_attachment(s, buf, strlen(buf), //adauga atasament la raspuns
                             SOAP_MIME_NONE, "text/plain",
                             NULL, // id, poate fi omis
                             "SOAP_serv_files.txt", // numele/calea fisierului
                             "informatii legate de toate fisierele stocate pe server-ul de SOAP" // descriere
    );
    close(fdorig);
   // unlink("temp.txt");
    rsp->out = ":: server :: Informatii legate de toate fisierele stocate pe server-ul SOAP";
    return SOAP_OK;
}