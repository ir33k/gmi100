#include <stdio.h>    /* Gemini CLI protocol client written in 100 lines of C */
#include <string.h>   /* v3.2 https://github.com/ir33k/gmi100 by irek@gabr.pl */
#include <unistd.h>   /* This is free and unencumbered software released into */
#include <netdb.h>    /* the public domain.  Read more: https://unlicense.org */
#include <err.h>
#include <openssl/ssl.h>
#define WARN(msg) do { fputs("WARNING: "msg"\n", stderr); goto start; } while(0)

int main(int argc, char **argv) {
        char uri[1024+1], buf[1024+1], buf2[1024+1], *bp, *t=tmpnam(0);
        int i, j, siz, sfd=0, hp, KB=1024;
        FILE *his, *tmp;
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl=0;
        addr.sin_family=AF_INET, addr.sin_port=htons(1965); /* Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) errx(1, "SSL_CTX_new");
        if (!(his = fopen(".gmi100", "a+b"))) err(1, "fopen(.gmi100)");
        if (!(tmp = fopen(t, "w+b"))) err(1, "fopen(%s)", t);
        fseek(his, 0, SEEK_END), hp = ftell(his)-1;
start:  fprintf(stderr, "gmi100> ");                             /* Main loop */
        if (!fgets(buf, KB, stdin)) return 0;
        if (*buf == '!') {                                             /* Cmd */
                buf[strlen(buf)-1] = 0;
sys:            sprintf(buf2, "%.256s %s", buf+1, t);
                system(buf2);
                goto start;
        } else if (buf[1]=='\n') switch (buf[0]) {                /* Commands */
        case 'q': return 0;
        case '?': sprintf(uri, "geminispace.info/search"); goto query;
        case 'r': strcpy(buf, uri); goto uri; /* Refresh */
        case 'c': printf("%s\n", uri); goto start;
        case 'u': sprintf(buf, "%.1021s../", uri); goto uri;
        case 'b': while (!fseek(his, --hp, 0) && hp && fgetc(his)!='\n');
                fgets(buf, KB, his);
                goto uri;
        }
        hp = ftell(his)-1; /* Reset history position */
        if ((i=atoi(buf)) > 0 && (siz=sprintf(buf, "[%d]\t=> ", i))) { /* Nav */
                rewind(tmp);
                for (bp=buf2; fgets(bp, siz+1, tmp) && (i=strcmp(bp, buf)););
                if (i) goto start;
                fgets(bp, KB, tmp);
                if (strstr(bp, "//")) uri[0] = 0; /* Absolute */
                else if (bp[0] == '/') uri[strcspn(uri, "/\n\0")] = 0;
                else if (!strncmp(&bp[i], "../", 2)); /* Keep whole uri */
                else if (!strchr(uri, '/')) strcat(uri, "/");
                else for(j = strlen(uri); j && uri[--j] != '/'; uri[j] = 0);
                sprintf(buf, "%s%s", uri, bp);
        }                                                        /* Typed URL */
uri:    i = strstr(buf, "//") ? (strncmp(buf, "gemini:", 7) ? 2 : 9) : 0;
        for (j=strlen(buf)-1; j>=0 && buf[j] <= ' '; j--); /* Trim */
        for (buf[j+1]=0, j=0, uri[0]=0; buf[i] && j<KB; uri[j]=0, i++) {
                if (!strncmp(&buf[i], "../", 2)) for (i+=2; uri[--j-1] != '/';);
                else if (buf[i] != ' ') uri[j++] = buf[i];
                else if ((j+=3) < KB) strcat(uri, "%20");
        }
        fprintf(stderr, "gemini://%s\n", uri);
        if (sfd && close(sfd) == -1) err(1, "close");
        if ((sfd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) WARN("socket");
        sprintf(buf, "%.*s", (int)strcspn(uri, ":/\0"), uri);
        if ((he = gethostbyname(buf)) == 0) WARN("Invalid host");
        for (i=0, j=1; j && he->h_addr_list[i]; i++) {
                memcpy(&addr.sin_addr.s_addr, he->h_addr_list[i], sizeof(in_addr_t);
                j = connect(sfd, (struct sockaddr*)&addr, sizeof(addr));
        }
        if (j) WARN("Failed to connect");
        siz = sprintf(buf2, "gemini://%.*s\r\n", KB, uri);
        if (ssl) SSL_free(ssl);
        if ((ssl = SSL_new(ctx)) == 0)           WARN("SSL_new");
        if (!SSL_set_tlsext_host_name(ssl, buf)) WARN("SSL_set_tlsext");
        if (!SSL_set_fd(ssl, sfd))               WARN("SSL_set_fd");
        if (SSL_connect(ssl) < 1)                WARN("SSL_connect");
        if (SSL_write(ssl, buf2, siz) < 1)       WARN("SSL_write");
        for (i=0, bp=buf2; SSL_read(ssl, bp, 1) && *bp != '\n'; bp+=1, bp[1]=0);
        if ((bp=buf2) && bp[0] == '1') {                             /* Query */
query:          siz = sprintf(buf, "%.*s?", (int)strcspn(uri, "?\0"), uri);
                printf("Query: ");
                fgets(buf+siz, KB-siz, stdin);
                goto uri;
        } else if (bp[0] == '3' && strcpy(buf, bp+3)) goto uri;   /* Redirect */
        fprintf(his, "%s\n", uri); /* Save URI in history */
        if (!(tmp = freopen(t, "w+b", tmp))) err(1, "freopen(%s)", t);
nodesc: if ((j=!strncmp(bp+3, "text/", 5))) while (SSL_peek(ssl, bp, 2)) {
                if (!strncmp(bp, "=>", 2) && SSL_read(ssl, bp, 2)) {
                        fprintf(tmp, "[%d]\t=> ", ++i); /* It's-a Mee, URIoo! */
                        while (SSL_peek(ssl,bp,1)&&*bp<=' ') SSL_read(ssl,bp,1);
                        while (SSL_read(ssl,bp,1)&& fputc(*bp,tmp) &&*bp> ' ');
                        if (*bp == '\n') goto nodesc; /* URI without descri */
                        fputs("\n\t", tmp);
                        while (SSL_peek(ssl,bp,1)&&*bp<=' ') SSL_read(ssl,bp,1);
                } while (SSL_read(ssl, bp, 1) && fputc(*bp, tmp) && *bp!='\n');
        } else while ((siz=SSL_read(ssl, bp, KB))) fwrite(bp, 1, siz, tmp);
        if (fflush(tmp)) err(1, "fflush(tmp)");
        if (j && sprintf(buf, "?%.256s", argc>1?argv[1]:"less -XI")) goto sys;
        goto start;
        return 0;
}
