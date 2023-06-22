#include <stdio.h>              /* Gemini CLI web client.                                 100x100 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <openssl/ssl.h>

#define MAXW    72              /* CLI maximum number of displayed characters per line*/
#define MAXH    20              /* CLI maximum number of displayed lines per page */
#define URISIZ  1024            /* Gemini maximum URI size */

char *net_err[] = {             /* Map h_errno to string, based on netdb.h srouce. */
    "", "HOST_NOT_FOUND", "TRY_AGAIN", "NO_RECOVERY", "NO_DATA"
};
char *ssl_err[] = {             /* Map SSL_get_error to string, based on ssl.h source. */
    "NONE", "SSL", "WANT_READ", "WANT_WRITE", "WANT_X509_LOOKUP", "SYSCALL", "ZERO_RETURN",
    "WANT_CONNECT", "WANT_ACCEPT", "WANT_ASYNC", "WANT_ASYNC_JOB", "WANT_CLIENT_HELLO_CB"
};

int main(void) {
    char input[URISIZ+1], buf[BUFSIZ], *line;
    int i, siz, sfd, uri, err;
    struct hostent *he;
    struct sockaddr_in addr;
    SSL_CTX *ctx;
    SSL *ssl;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1965); /* Hardcoded Gemini port */
    SSL_library_init();
    if (!(ctx = SSL_CTX_new(TLS_client_method()))) {
        fprintf(stderr, "ERROR: SSL_CTX_new failed");
        return 1;
    }
    while(scanf("%1024s", input) != EOF) {             /* Get input from user */
        if (input[0] && !input[1]) switch (input[0]) { /* Handle one letter commands */
            case 'q': goto quit;                       /* Quit program */
            case 'b': printf("Go back\n");   continue; /* TODO */
            case 'r': printf("Refresh\n");   continue; /* TODO */
            case 'n': printf("Next page\n"); continue; /* TODO */
            case 'p': printf("Prev page\n"); continue; /* TODO */
            case 'h': printf("Help page\n"); continue; /* TODO */
        } else if ((uri = atoi(input)) > 0) {          /* Handle navigation URI navigation */
            printf("Navigation to: %d\n", uri);        /* TODO */
            continue;
        } while (1) {           /* Else this is an URI string. */
            if ((sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) goto errstd;
            if ((he = gethostbyname(input)) == 0) goto errnet;
            for (i = 0; he->h_addr_list[i]; i++) {
                addr.sin_addr.s_addr = *((unsigned long *)he->h_addr_list[i]);
                err = connect(sfd, (struct sockaddr *)&addr, sizeof(addr));
                if (err == 0) break;
            }
            if (err) goto errstd;
            siz = sprintf(buf, "gemini://%s\r\n", input);
            if ((ssl = SSL_new(ctx)) == 0)             goto errssl;
            if ((err = SSL_set_fd(ssl, sfd)) == 0)     goto errssl;
            if ((err = SSL_connect(ssl)) < 0)          goto errssl;
            if ((err = SSL_write(ssl, buf, siz)) <= 0) goto errssl;
            while ((siz = SSL_read(ssl, buf, BUFSIZ-1)) > 0) {
                buf[siz] = 0;
                line = strtok(buf, "\n");
                do {
                    printf("%s\n", line);
                } while ((line = strtok(0, "\n")));
            }
            close(sfd);
            SSL_free(ssl);      /* SSL_shutdown skipped on purpose */
            break;
        }
        continue;
errstd: fprintf(stderr, "ERROR: STD %s\n", strerror(errno)); continue;
errnet: fprintf(stderr, "ERROR: NET %s\n", net_err[h_errno]); continue;
errssl: fprintf(stderr, "ERROR: SSL %s\n", ssl_err[SSL_get_error(ssl, err)]); continue;
    }
quit:
    SSL_CTX_free(ctx);
    return 0;
}
