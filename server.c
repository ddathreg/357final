#define _GNU_SOURCE
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

// #define PORT 2828

void sigchild_h(int sig) {
   while (waitpid(-1, NULL, WNOHANG) > 0) {
   }
}

void handle_request(int nfd)
{
   // printf("Child Connection Established\n");
   // printf("HERE???");

   FILE *network = fdopen(nfd, "r+");
   char *line = NULL;
   char line1[1024];
   size_t size;
   ssize_t num;
   // printf("here?");

   if (network == NULL)
   {
      printf("Error: fdopen");
      close(nfd);
      return;
   }

   char *type = NULL;
   char *filename = NULL;
   // printf("here");

   while ((num = getline(&line, &size, network)) >= 0) {
      // printf("line: %s\n", line);
      strcpy(line1, line);
      char *token = NULL;
      const char *delim = " ";

      token = strtok(line1, delim); 
      type = token;

      
      token = strtok(NULL, delim);
      filename = token;
      // printf("type: %s filename: %s\n", type, filename);

      if (type == NULL || filename == NULL) {
         fprintf(network, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 37\r\n\r\n<html><body>Bad Request</body></html>");
         continue;
      } 
      if (type != "HEAD" && type != "GET") {
         fprintf(network, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 37\r\n\r\n<html><body>Bad Request</body></html>");
         continue;
      }

      // send reply and cgi stuff
      
      if (strncmp(filename, "/cgi-like/", 10) == 0) {
         // printf("1\n");
         char fn[1024];
         strcpy(fn, filename);
         // char *cgifile = filename;
         char *cgifile = strtok(fn, "?");
         // printf("cgi: %s\n", cgifile);

         char *args[30];
         char *ncgi = cgifile + 10;
         args[0] = ncgi;
         int idx = 1;
         char *tok = NULL;
         while ((tok = strtok(NULL, "&")) != NULL) {
            if (idx < 30 && (strncmp(tok, "..", 2) != 0)) {
               args[idx] = tok; 
               idx++;
            }
         }
         printf("%d", idx);
         // char *arglist[] = {cgifile+10, args};

         // for (int i = 0; i < idx; i++) {
         //    printf("arg: %s\n", args[i]);
         // }
         // args[idx] = NULL;
         // printf("%s\n", args[1]);
         
         struct stat st;
         int statres = stat(cgifile + 1, &st);
         if (statres != 0) {
            fprintf(network, "HTTP/1.0 403 Permission Denied\r\nContent-Type: text/html\r\nContent-Length: 43\r\n\r\n<html><body>Permission Denied</body></html>");
         } else {
            pid_t pid = fork();
            if (pid < 0) {
               fprintf(network, "HTTP/1.0 500 Internal Error\r\nContent-Type: text/html\r\nContent-Length: 39\r\n\r\n<html><body>Internal Error</body></html>");
            } else if (pid == 0) {
               char tf[30];
               sprintf(tf, "file_%d", getpid());
               // printf("tf: %s\n", tf);
               int tfile = open(tf, O_RDWR | O_TRUNC | O_CREAT, 0600);
               dup2(tfile, 1);
               close(tfile);
               // printf("+9: %s\n", cgifile+10);
               execvp(cgifile + 10, args);
               fprintf(network, "HTTP/1.0 500 Internal Error\r\nContent-Type: text/html\r\nContent-Length: 39\r\n\r\n<html><body>Internal Error</body></html>");
            } else {
               waitpid(pid, NULL, 0);
               char ntf[30];
               sprintf(ntf, "file_%d", pid);
               // printf("ntf: %s\n", ntf);
               struct stat st2;
               int st2s = stat(ntf, &st2);
               if (st2s != 0) {
                  fprintf(network, "HTTP/1.0 500 Internal Error\r\nContent-Type: text/html\r\nContent-Length: 39\r\n\r\n<html><body>Internal Error</body></html>");
               } else {
                  fprintf(network, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %lld\r\n\r\n", st2.st_size);
                  int nf = open(ntf, O_RDONLY);
                  char *con = malloc(st2.st_size);
                  read(nf, con, st2.st_size);
                  fprintf(network, "%s", con);
                  close(nf);
                  remove(ntf);
                  free(con);
               }
            }
         }


      } else {
         struct stat st;
         int statres = stat(filename + 1, &st);
         if (statres != 0) {
            fprintf(network, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 35\r\n\r\n<html><body>Not Found</body></html>");
         } else {
            if (S_ISREG(st.st_mode)) {
               char header[1024];
               sprintf(header, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %lld\r\n\r\n", st.st_size);

               fprintf(network, "%s", header);

               if (strcmp(type, "GET") == 0) {
                  int file = open(filename + 1, O_RDONLY);
                  char buf[1024];
                  while ((read(file, buf, sizeof(buf))) > 0) {
                     fprintf(network, "%s", buf);
                  }
                  close(file);
               }
            } else {
               fprintf(network, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 37\r\n\r\n<html><body>Bad Request</body></html>");
            }
         }
      }
   }
   fclose(network);
}

void run_service(int fd)
{
   while (1)
   {
      int nfd = accept_connection(fd);
      if (nfd != -1)
      {
         // printf("Connection established\n");
         pid_t schild = fork();

         if (schild < 0) {
         printf("Error: fork");
         } else if (schild == 0) {
            handle_request(nfd);
            close(nfd);
         } else {
            struct sigaction sa;
            sa.sa_handler = sigchild_h;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
            if (sigaction(SIGCHLD, &sa, NULL) == -1) {
               printf("Error: sa");
               
            }
            // kill(schild, SIGKILL);
         }
      }
   }
}


int main(int argc, char *argv[])
{  
   if (argc != 2) {
      printf("Error: arg count");
      return 1;
   }

   int PORT = atoi(argv[1]);
   

   int fd = create_service(PORT);

   if (fd == -1)
   {
      perror(0);
      exit(1);
   }

   printf("listening on port: %d\n", PORT);

   run_service(fd);
   close(fd);
   printf("Connection closed\n");


   return 0;
}
