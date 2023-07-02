#include <stdio.h>              /* Gemini CLI web client in 100 lines of c89  ^-^         100x100 */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <openssl/ssl.h>        /* Add "-lssl -lcrypto" compilation flags to link openssl lib */
#define KB 1024                 /* One kilobyte is the max size of Gemini URI */

/* Map h_errno to string, based on netdb.h srouce. */
static const char *neterr[] = { "", "HOST_NOT_FOUND", "TRY_AGAIN", "NO_RECOVERY", "NO_DATA" };

int main(void) {
        char uri[KB+1], tmp[KB+1], *buf, *bp, *next, bool;
        int i, j, siz, sfd, err, bsiz;
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;
        bsiz = 2*sysconf(_SC_PAGESIZE);                       /* 2 pages of buffer memory*/
        buf = malloc(bsiz);                                   /* There is no free */
        addr.sin_family = AF_INET;                            /* Use "Internet" address family */
        addr.sin_port = htons(1965);                          /* Hardcoded Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) {
                fprintf(stderr, "ERROR: SSL_CTX_new failed");
                return 1;
        }
        while(1) {                                            /* Main loop */
                printf("gmi100: ");                           /* Prompt for input */
                if (!fgets(tmp, KB, stdin)) break;            /* Get one line of input from user */
                if (tmp[1] == '\n') switch (tmp[0]) {         /* Handle commands */
                        case 'q': goto quit;                  /* Quit */
                        case 'b': printf("go back\n"); continue; /* TODO */
                } if ((i = atoi(tmp)) > 0) {                  /* Handle URI navigation */
                        for (next = buf; i && next; next += 3, i--)
                                next = strstr(next, "\n=>");
                        if (i) {
                                fprintf(stderr, "ERROR: URI not found\n");
                                continue;
                        }
                        for (; next[0] <= ' '; next += 1);    /* Skip whitespaces */
                        next[strcspn(next, " \t\n\0")] = 0;   /* Mark end with null terminator */
                        if (strncmp(next, "gemini://", 9)) strcat(uri, next);
                        else sprintf(uri, "%s", next);
                        
                } else strcpy(uri, tmp);                      /* Handle typed URL */
start:          for (i=0, j=0; uri[i] && i < KB && j < KB; i++) {
                        if (uri[i] == '\n' || uri[i] == '\r') continue;
                        if (uri[i] == ' ' && (j+=3) < KB) strcat(tmp, "%20");
                        else tmp[j++] = uri[i];
                }
                sprintf(uri, "%.*s", j, tmp);
                fprintf(stderr, "@cli: request\t\"%s\"\n", uri);
                if ((sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) goto errstd;
                bool = strncmp(uri, "gemini://", 9);               /* URI strarts with protocol */
                bp = uri + 9 * !bool;                              /* Remove URI protocol */
                sprintf(tmp, "%.*s", (int)strcspn(bp, "/\0"), bp); /* Extract URI host */
                if ((he = gethostbyname(tmp)) == 0) goto errnet;
                siz = sizeof(addr);
                for (i = 0; he->h_addr_list[i]; i++) {
                        addr.sin_addr.s_addr = *((unsigned long*)he->h_addr_list[i]);
                        if ((err = connect(sfd, (struct sockaddr*)&addr, siz)) == 0) break;
                }
                if (err) goto errstd;
                siz = sprintf(buf, "%s%.*s\r\n", bool ? "gemini://" : "", KB, uri);
                if ((ssl = SSL_new(ctx)) == 0)             goto errssl;
                if ((err = SSL_set_fd(ssl, sfd)) == 0)     goto errssl;
                if ((err = SSL_connect(ssl)) < 0)          goto errssl;
                if ((err = SSL_write(ssl, buf, siz)) <= 0) goto errssl;
                uri[strcspn(uri, "?\0")] = 0; /* Remove old query from URI */
                for (siz=0; bsiz-siz-1 > 0 && (err = SSL_read(ssl, buf+siz, bsiz-siz-1)); siz+=err);
                buf[siz] = 0;
                bp = buf;
                next = strchr(bp, '\r');
                sprintf(tmp, "%.*s", (int)(next-bp), bp);
                bp = next+1;
                fprintf(stderr, "@cli: response\t\"%s\"\n", tmp);
                if (buf[0] == '1') {                                       /* Prompt */
                        printf("Query: ");
                        strcat(uri, "?");
                        siz = strlen(uri);
                        fgets(uri+siz, KB-siz, stdin);
                        goto start;
                } else if (buf[0] == '3') {                                /* Redirect */
                        sprintf(uri, "%.*s", KB, tmp +3);
                        goto start;
                } else for (i=1; (next = strchr(bp, '\n')); bp = next+1) { /* Print */
                        if (!strncmp(bp, "=>", 2)) printf("[%d]\t", i++);  /* It's-a Me, URIoo! */
                        printf("%.*s\n", (int)(next-bp), bp);
                }
                close(sfd);
                SSL_free(ssl); /* SSL_shutdown skipped on purpose */         continue;
errstd:         fprintf(stderr, "ERROR: STD %s\n", strerror(errno));         continue;
errnet:         fprintf(stderr, "ERROR: NET %s\n", neterr[h_errno]);         continue;
errssl:         fprintf(stderr, "ERROR: SSL %d\n", SSL_get_error(ssl, err)); continue;
        }
quit:   SSL_CTX_free(ctx);
        return 0;
}
