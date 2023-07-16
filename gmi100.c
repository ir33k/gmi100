#include <stdio.h>    /* Gemini CLI protocol client written in 100 lines of C */
#include <string.h>   /* v3.0 https://github.com/ir33k/gmi100 by irek@gabr.pl */
#include <unistd.h>   /* This is free and unencumbered software released into */
#include <netdb.h>    /* the public domain.  Read more: https://unlicense.org */
#include <err.h>
#include <openssl/ssl.h>
#define WARN(msg) do { fputs("WARNING: "msg"\n", stderr); goto start; } while(0)

int main(int argc, char **argv) {
        char uri[1024+1], buf[1024+1], buf2[1024+1], *bp, *p;
        int i, j, siz, sfd, hp, KB=1024;
        FILE *history, *tmp;
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;

        addr.sin_family = AF_INET;
        addr.sin_port = htons(1965); /* Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) errx(1, "SSL_CTX_new");
        if (!(history = fopen(".gmi100", "a+b"))) err(1, "fopen(.gmi100)");
        fseek(history, 0, SEEK_END);
        hp = ftell(history)-1;
start:  fprintf(stderr, "gmi100> ");                             /* Main loop */
        if (!fgets(buf, KB, stdin)) goto quit;
        /* TODO(irek): I could have access to search engine with ? prefix. */
        /* TOOD(irek): It would be great to support local files. */
        if (buf[1]=='\n') switch (buf[0]) {                       /* Commands */
        case 'q': goto quit;
        case 'c': printf("%s\n", uri); goto start;
        case 'r': strcpy(buf, uri); goto uri;
        case 'B': sprintf(buf, "%.1021s../", uri); goto uri;
        case 'b':
                while (!fseek(history, --hp, 0) && hp && fgetc(history)!='\n');
                fgets(buf, KB, history);
                goto uri;
        }
        hp = ftell(history)-1; /* Reset history position */
        if ((i = atoi(buf)) > 0) {                              /* Navigation */
                siz = sprintf(buf, "[%d]\t=>", i);
                if (!(tmp = fopen(p, "rb"))) err(1, "fopen(%s)", p);
                for (bp=buf2; fgets(bp,siz,tmp) && i;) if (!strcmp(bp,buf)) i--;
                if (i || !bp) {
                        printf(">>>> not found %d", i);
                        if (fclose(tmp) == EOF) err(1, "fclose(tmp)");
                        goto start;
                }
                while ((j = fgetc(tmp)) <= ' ');
                ungetc(j, tmp);
                fgets(bp, KB, tmp);
                if (fclose(tmp) == EOF) err(1, "fclose(tmp)");
                bp[strcspn(bp, " \t\n\0")] = 0;
                if (strstr(bp, "//")) uri[0] = 0; /* Absolute */
                else if (bp[0] == '/') uri[strcspn(uri, "/\n\0")] = 0;
                else if (!strncmp(&bp[i], "../", 2)); /* Keep whole uri */
                else if (!strchr(uri, '/')) strcat(uri, "/");
                else for(j = strlen(uri); j && uri[--j] != '/'; uri[j] = 0);
                sprintf(buf, "%s%s", uri, bp);
        }                                                        /* Typed URL */
uri:    i = strstr(buf, "//") ? (strncmp(buf, "gemini:", 7) ? 2 : 9) : 0;
        for (j=strlen(buf)-1; buf[j] <= ' '; j--); /* Trim */
        for (buf[j+1]=0, j=0; buf[i] && j<KB; uri[j]=0, i++) {
                if (!strncmp(&buf[i], "../", 2)) for (i+=2; uri[--j-1] != '/';);
                else if (buf[i] != ' ') uri[j++] = buf[i];
                else if ((j+=3) < KB) strcat(uri, "%20");
        }
        fprintf(stderr, "gemini://%s\n", uri);
        if ((sfd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) WARN("socket");
        sprintf(buf, "%.*s", (int)strcspn(uri, ":/\0"), uri);
        if ((he = gethostbyname(buf)) == 0) WARN("Invalid host");
        for (i=0, j=1; j && he->h_addr_list[i]; i++) {
                addr.sin_addr.s_addr = *((unsigned long*)he->h_addr_list[i]);
                j = connect(sfd, (struct sockaddr*)&addr, sizeof(addr));
        }
        if (j) WARN("Failed to connect");
        siz = sprintf(buf2, "gemini://%.*s\r\n", KB, uri);
        if ((ssl = SSL_new(ctx)) == 0)           WARN("SSL_new");
        if (!SSL_set_tlsext_host_name(ssl, buf)) WARN("SSL_set_tlsext");
        if (!SSL_set_fd(ssl, sfd))               WARN("SSL_set_fd");
        if (SSL_connect(ssl) < 1)                WARN("SSL_connect");
        if (SSL_write(ssl, buf2, siz) < 1)       WARN("SSL_write");
        if (!(tmp = fopen(p=tmpnam(0), "w+b"))) err(1, "fopen(%s)", p);
        for (i=0, bp=buf2; SSL_read(ssl, bp, 1) && *bp != '\n'; bp+=1, bp[1]=0);
        if ((j=!strncmp(buf2+3, "text/", 5))) while (SSL_peek(ssl, buf, 2)) {
                if (!strncmp(buf, "=>", 2)) fprintf(tmp, "[%d]\t", ++i);
                while (SSL_read(ssl, buf, 1) && fputc(*buf, tmp) && *buf!='\n');
        } else while ((siz=SSL_read(ssl, bp, 1024))) fwrite(bp, 1, siz, tmp);
        if (fclose(tmp) == EOF) err(1, "fclose(tmp)");
        SSL_free(ssl); /* No SSL_shutdown by design */
        if ((bp=buf2) && bp[0] == '1') {                             /* Query */
                siz = sprintf(buf, "%.*s?", (int)strcspn(uri, "?\0"), uri);
                printf("Query: ");
                fgets(buf+siz, KB-siz, stdin);
                goto uri;
        } else if (bp[0] == '3' && strcpy(buf, bp+3)) goto uri;   /* Redirect */
        fprintf(history, "%s\n", uri); /* Save URI in history */
        sprintf(buf, "%.128s %s", j?(argc>1?argv[1]:"less -XI"):"xdg-open", p);
        system(buf); /* Print using pager or open file */
        goto start;
quit:   if (fclose(history) == EOF) err(1, "fclose(history)");
        return 0;
}
