#include <stdio.h>              /* Gemini CLI web client in 100 lines of c89  ^-^         100x100 */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <openssl/ssl.h>        /* Add "-lssl -lcrypto" compilation flags to link openssl lib     */
#define KB 1024                 /* One kilobyte is the max size of Gemini URI                     */

/* Map h_errno to string, based on netdb.h srouce. */
static const char *neterr[] = { "", "HOST_NOT_FOUND", "TRY_AGAIN", "NO_RECOVERY", "NO_DATA" };

int main(void) {
        char uri[KB+1], tmp[KB+1], *buf, *bp, *next, bool;
        int i, j, siz, sfd, err, bsiz;
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;
        bsiz = 2*sysconf(_SC_PAGESIZE);                       /* Use 2 pages for buffer memory    */
        buf = malloc(bsiz);                                   /* Intentinally there is no free()  */
        addr.sin_family = AF_INET;                            /* Use "Internet" address family    */
        addr.sin_port = htons(1965);                          /* Hardcoded Gemini port            */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) {
                fprintf(stderr, "ERROR: SSL_CTX_new failed");
                return 1;                                     /* Fail, can't work without SSL     */
        }
start:  printf("gmi100: ");                                   /* Prompt, start of main loop       */
        if (!fgets(tmp, KB, stdin)) goto quit;                /* Get one line of input from user  */
        if (tmp[1] == '\n') switch (tmp[0]) {                 /* Handle commands                  */
                case 'q': goto quit;                          /* Quit                             */
                case 'b': goto start;;                        /* TODO(irek): Go back              */
        } if ((i = atoi(tmp)) > 0) {                          /* Handle URI navigation            */
                for (bp = buf; i && bp; i--)                  /* Search whole buffer for links    */
                        bp = strstr(bp+3, "\n=>");            /* Jump from link to link           */
                if (i || !bp) goto start;                     /* Wrong URI index, try again       */
                for (bp += 3; *bp <= ' '; bp += 1);           /* Skip whitespaces                 */
                bp[strcspn(bp, " \t\n\0")] = 0;               /* Mark end with null terminator    */
                if (strncmp(bp, "gemini://", 9)) strcat(uri, bp); /* Relative URI                 */
                else strcpy(uri, bp);                         /* Absolute URI                     */
        } else strcpy(uri, tmp);                              /* Handle URL typed by hand         */
uri:    for (i=j=0; uri[i] && i<KB && j<KB; tmp[j]=0, i++) {  /* Normalize URI                    */
                if (uri[i] == '\n') continue;                 /* Skip new line characters         */
                if (uri[i] != ' ') tmp[j++] = uri[i];         /* Copy regular characters          */
                else if ((j+=3) < KB) strcat(tmp, "%20");     /* Replace whitepsace whit %20      */
        }
        sprintf(uri, "%.*s", j, tmp);
        fprintf(stderr, "@cli: request\t\"%s\"\n", uri);
        if ((sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) goto errstd;
        bool = strncmp(uri, "gemini://", 9);                  /* URI strarts with protocol        */
        bp = uri + 9 * !bool;                                 /* Remove URI protocol              */
        sprintf(tmp, "%.*s", (int)strcspn(bp, "/\0"), bp);    /* Extract URI host                 */
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
        uri[strcspn(uri, "?\0")] = 0;                         /* Remove old query from URI        */
        for (siz=0; bsiz-siz-1 > 0 && (err = SSL_read(ssl, buf+siz, bsiz-siz-1)); siz+=err);
        buf[siz] = 0;
        bp = buf;
        next = strchr(bp, '\r');
        sprintf(tmp, "%.*s", (int)(next-bp), bp);
        bp = next+1;
        fprintf(stderr, "@cli: response\t\"%s\"\n", tmp);
        if (buf[0] == '1') {                                  /* Prompt                           */
                printf("Query: ");
                strcat(uri, "?");
                siz = strlen(uri);
                fgets(uri+siz, KB-siz, stdin);
                goto uri;
        } else if (buf[0] == '3') {                           /* Redirect                         */
                sprintf(uri, "%.*s", KB, tmp +3);
                goto uri;
        } for (i=1; (next = strchr(bp, '\n')); bp = next+1) { /* Print                            */
                if (!strncmp(bp, "=>", 2)) printf("[%d]\t", i++); /* It's-a Mee, URIoooo!         */
                printf("%.*s\n", (int)(next-bp), bp);
        }
        close(sfd);
        SSL_free(ssl); /* SSL_shutdown skipped on purpose */         goto start;
errstd: fprintf(stderr, "ERROR: STD %s\n", strerror(errno));         goto start;
errnet: fprintf(stderr, "ERROR: NET %s\n", neterr[h_errno]);         goto start;
errssl: fprintf(stderr, "ERROR: SSL %d\n", SSL_get_error(ssl, err)); goto start;
quit:   SSL_CTX_free(ctx);
        return 0;
}
