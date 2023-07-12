#include <stdio.h>    /* Gemini CLI protocol client written in 100 lines of C */
#include <string.h>   /* v2.0 https://github.com/ir33k/gmi100 by irek@gabr.pl */
#include <unistd.h>   /* This is free and unencumbered software released into */
#include <netdb.h>    /* the public domain.  Read more: https://unlicense.org */
#include <err.h>
#include <openssl/ssl.h>

#define WARN(msg) do { fputs("WARNING: "msg"\n", stderr); goto start; } while(0)

int main(void) {
        char uri[1024+1], tmp[1024+1], *buf, *bp=0;
        int i, j, siz, bsiz, sfd, back, KB=1024;
        FILE *fp; /* History file */
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;

        buf = malloc((bsiz = 4*sysconf(_SC_PAGESIZE)));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1965); /* Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) errx(1, "SSL_CTX_new");
        if (!(fp = fopen(".gmi100", "a+b"))) err(1, "fopen(.gmi100)");
        fseek(fp, 0, SEEK_END);
        back = ftell(fp)-1;
start:  fprintf(stderr, "gmi100> ");                /* PROMPT Start main loop */
        if (!fgets(tmp, KB, stdin)) goto quit;
        if (tmp[0]=='\n' || tmp[1]=='\n') switch (tmp[0]) {    /* 1: Commands */
        case 'q': case 'c':  case 'x': goto quit;
        case 'r': case '0':  case 'k': strcpy(tmp, uri); goto uri; /* Refresh */
        case 'b': case 'p':  case 'h':
                while (!fseek(fp, --back, 0) && back && fgetc(fp)!='\n');
                fgets(tmp, KB, fp);
                goto uri;
        }
        back = ftell(fp)-1; /* Reset history position */
        if ((i = atoi(tmp)) > 0) {                           /* 2: Navigation */
                for (bp = buf; i && bp; i--) bp = strstr(bp+3, "\n=>");
                if (i || !bp) goto start;
                for (bp += 3; *bp <= ' '; bp += 1);
                bp[strcspn(bp, " \t\n\0")] = 0;
                if (strstr(bp, "//")) uri[0] = 0;
                else if (bp[0] == '/') uri[strcspn(uri, "/\n\0")] = 0;
                else if (!strncmp(&bp[i], "../", 2)); /* Keep whole uri */
                else for(j = strlen(uri); j && uri[--j] != '/'; uri[j] = 0);
                sprintf(tmp, "%s%s", uri, bp);
        }                                                     /* 3: Typed URL */
uri:    i = strstr(tmp, "//") ? (strncmp(tmp, "gemini:", 7) ? 2 : 9) : 0;
        for (j=strlen(tmp)-1; tmp[j] <= ' '; j--); /* Trim */
        for (tmp[j+1]=0, j=0; tmp[i] && j<KB; uri[j]=0, i++) {
                if (!strncmp(&tmp[i], "../", 2)) for (i+=2; uri[--j-1] != '/';);
                else if (tmp[i] != ' ') uri[j++] = tmp[i];
                else if ((j+=3) < KB) strcat(uri, "%20");
        }
        fprintf(stderr, "%s\n", uri);
        if ((sfd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) WARN("socket");
        sprintf(tmp, "%.*s", (int)strcspn(uri, ":/\0"), uri);
        if ((he = gethostbyname(tmp)) == 0) WARN("gethostbyname");
        for (i=0; he->h_addr_list[i]; i++) {
                addr.sin_addr.s_addr = *((unsigned long*)he->h_addr_list[i]);
                j = connect(sfd, (struct sockaddr*)&addr, sizeof(addr));
                if (!j) break; /* Success */
        }
        if (j) WARN("Failed to connect");
        siz = sprintf(buf, "gemini://%.*s\r\n", KB, uri);
        if ((ssl = SSL_new(ctx)) == 0)          WARN("SSL_new");
        if ((j = SSL_set_fd(ssl, sfd)) == 0)    WARN("SSL_set_fd");
        if ((j = SSL_connect(ssl)) < 0)         WARN("SSL_connect");
        if ((j = SSL_write(ssl, buf, siz)) < 1) WARN("SSL_write");
        for (bp=buf, siz=0; bsiz-siz-1 > 0 && j; bp[siz+=j]=0)
                j = SSL_read(ssl, buf+siz, bsiz-siz-1);
        SSL_free(ssl); /* No SSL_shutdown by design */
        if (buf[0] == '1') {                                         /* Query */
                siz = sprintf(tmp, "%.*s?", (int)strcspn(uri, "?\0"), uri);
                printf("Query: ");
                fgets(tmp+siz, KB-siz, stdin);
                goto uri;
        } else if (buf[0] == '3') {                               /* Redirect */
                sprintf(tmp, "%.*s", (int)strcspn(bp+3, "\r\n"), bp+3);
                goto uri;
        } for (i=0; *bp && (siz=strcspn(bp, "\n\0")) > -1; bp+=siz+1) {
                if (!strncmp(bp, "=>", 2)) { /* It's-a Mee, URIoo! */
                        siz = strcspn((bp += 2+strspn(bp+2, " \t")), " \t\n\0");
                        printf("[%d]\t%.*s\n\t", ++i, siz, bp);
                        siz += strspn(bp+siz, " \t")-1;
                } else printf("%.*s\n", siz, bp);
        }
        fprintf(fp, "%s\n", uri); /* Append URI to history */
        goto start;
quit:   if (fclose(fp) == EOF) err(1, "fclose");
        return 0;
}
