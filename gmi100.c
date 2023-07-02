#include <stdio.h>              /* Gemini CLI web client in 100 lines.                    100x100 */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <openssl/ssl.h>

#define KB      "1024"
#define BSIZ    BUFSIZ          /* Buffer size */

char *net_err[] = {"", "HOST_NOT_FOUND", "TRY_AGAIN", "NO_RECOVERY", "NO_DATA"}; /* Src: netdb.h */
char *ssl_read_line(SSL *ssl, char *buf, size_t siz, char *prev, char *delim) {
        if (*delim) buf = prev + strlen(prev) +1;                  /* Get next line from old buf */
        else if ((siz = SSL_read(ssl, buf, siz-1)) <= 0) return 0; /* Get new buffer or end here */
        else buf[siz] = 0;                                         /* Get first line from new buf */
        siz = strcspn(buf, "\r\n\0");                              /* Find end of line */
        *delim = buf[siz];                                         /* Store line delimeter */
        buf[siz] = 0;                                              /* End line on delimeter */
        return buf;
}
int main(void) {
        char uri[BSIZ], buf[BSIZ], str[BSIZ], *bp, delim;
        int i, siz, sfd, err, bool;
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
        while(scanf("%"KB"s", uri) != EOF) {           /* Get input from user */
start:          if (uri[0] && !uri[1]) switch (uri[0]) {   /* Handle one letter commands */
                case 'q': goto quit;                       /* Quit program */
                case 'b': printf("Go back\n");   continue; /* TODO */
                } else if ((i = atoi(uri)) > 0) {          /* Handle URI navigation */
                        printf("Navigation to: %d\n", i);  /* TODO */
                        continue;
                } /* Else this is an URI string. */
                printf("@cli: requesting '%s'\n", uri);
                if ((sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) goto errstd;
                strcpy(buf, uri);
                bool = strncmp(buf, "gemini://", 9);               /* URI has protocol */
                bp = buf + 9 * !bool;                              /* Remove URI protocol */
                sprintf(str, "%.*s", (int)strcspn(bp, "/\0"), bp); /* Extract URI host */
                if ((he = gethostbyname(str)) == 0) goto errnet;
                for (i = 0; he->h_addr_list[i]; i++) {
                        addr.sin_addr.s_addr = *((unsigned long *)he->h_addr_list[i]);
                        if ((err = connect(sfd, (struct sockaddr *)&addr, sizeof(addr))) == 0) break;
                }
                if (err) goto errstd;
                siz = sprintf(str, "%s%."KB"s\r\n", bool ? "gemini://" : "", buf);
                if ((ssl = SSL_new(ctx)) == 0)             goto errssl;
                if ((err = SSL_set_fd(ssl, sfd)) == 0)     goto errssl;
                if ((err = SSL_connect(ssl)) < 0)          goto errssl;
                if ((err = SSL_write(ssl, str, siz)) <= 0) goto errssl;
                uri[strcspn(uri, "?\0")] = 0; /* Remove old query from URI */
                str[0] = 0, delim = 0;
                while (!delim && (bp = ssl_read_line(ssl, buf, BSIZ, bp, &delim))) strcat(str, bp);
                printf("@cli: response header '%s'\n", str);
                switch (str[0]) {
                case '1':       /* Prompt */
                        printf("Query: ");
                        scanf("%"KB"s", buf);
                        /* TODO(irek): Replace spaces */
                        sprintf(str, "   %."KB"s?%."KB"s", uri, buf);
                        /* fallthrough */
                case '3':       /* Redirect */
                        sprintf(uri, "%."KB"s", str +3);
                        goto start;
                }
                while ((bp = ssl_read_line(ssl, buf, BSIZ, bp, &delim))) {
                        printf("%s%s", bp, delim ? "\n" : "");
                }
                putc('\n', stdout);
                close(sfd);
                SSL_free(ssl);  /* SSL_shutdown skipped on purpose */
                continue;
errstd:         fprintf(stderr, "ERROR: STD %s\n", strerror(errno));         continue;
errnet:         fprintf(stderr, "ERROR: NET %s\n", net_err[errno]);          continue;
errssl:         fprintf(stderr, "ERROR: SSL %d\n", SSL_get_error(ssl, err)); continue;
        }
quit:   SSL_CTX_free(ctx);
        return 0;
}
