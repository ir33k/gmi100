#include <stdio.h>    /* Gemini CLI protocol client written in 100 lines of C */
#include <string.h>   /* v2.2 https://github.com/ir33k/gmi100 by irek@gabr.pl */
#include <unistd.h>   /* This is free and unencumbered software released into */
#include <netdb.h>    /* the public domain.  Read more: https://unlicense.org */
#include <err.h>
#include <openssl/ssl.h>

#define WARN(msg) do { fputs("WARNING: "msg"\n", stderr); goto start; } while(0)

int main(int argc, char **argv) {
        char uri[1024+1], buf[1024+1], *res, *bp=0, *PATH=tmpnam(0);
        int i, j, siz, rsiz, sfd, hp, KB=1024;
        FILE *history, *tmp;
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;

        res = malloc((rsiz = 4*sysconf(_SC_PAGESIZE)));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1965); /* Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) errx(1, "SSL_CTX_new");
        if (!(history = fopen(".gmi100", "a+b"))) err(1, "fopen(.gmi100)");
        fseek(history, 0, SEEK_END);
        hp = ftell(history)-1;
start:  fprintf(stderr, "gmi100> ");                /* PROMPT Start main loop */
        if (!fgets(buf, KB, stdin)) goto quit;
        if (buf[1]=='\n') switch (buf[0]) {                    /* 1: Commands */
        case 'q': goto quit;
        case '0': strcpy(buf, uri); goto uri; /* Refresh */
        case 'u': sprintf(buf, "%.1021s../", uri); goto uri; /* Up URI dir */
        case 'b': /* Go back in browsing history */
                while (!fseek(history, --hp, 0) && hp && fgetc(history)!='\n');
                fgets(buf, KB, history);
                goto uri;
        }
        hp = ftell(history)-1; /* Reset history position */
        if ((i = atoi(buf)) > 0) {                           /* 2: Navigation */
                for (bp = res; i && bp; i--) bp = strstr(bp+3, "\n=>");
                if (i || !bp) goto start;
                for (bp += 3; *bp <= ' '; bp += 1);
                bp[strcspn(bp, " \t\n\0")] = 0;
                if (strstr(bp, "//")) uri[0] = 0;
                else if (bp[0] == '/') uri[strcspn(uri, "/\n\0")] = 0;
                else if (!strncmp(&bp[i], "../", 2)); /* Keep whole uri */
                else for(j = strlen(uri); j && uri[--j] != '/'; uri[j] = 0);
                sprintf(buf, "%s%s", uri, bp);
        }                                                     /* 3: Typed URL */
uri:    i = strstr(buf, "//") ? (strncmp(buf, "gemini:", 7) ? 2 : 9) : 0;
        for (j=strlen(buf)-1; buf[j] <= ' '; j--); /* Trim */
        for (buf[j+1]=0, j=0; buf[i] && j<KB; uri[j]=0, i++) {
                if (!strncmp(&buf[i], "../", 2)) for (i+=2; uri[--j-1] != '/';);
                else if (buf[i] != ' ') uri[j++] = buf[i];
                else if ((j+=3) < KB) strcat(uri, "%20");
        }
        if ((sfd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) WARN("socket");
        sprintf(buf, "%.*s", (int)strcspn(uri, ":/\0"), uri);
        if ((he = gethostbyname(buf)) == 0) WARN("Invalid host");
        for (i=0, j=1; j && he->h_addr_list[i]; i++) {
                addr.sin_addr.s_addr = *((unsigned long*)he->h_addr_list[i]);
                j = connect(sfd, (struct sockaddr*)&addr, sizeof(addr));
        }
        if (j) WARN("Failed to connect");
        siz = sprintf(res, "gemini://%.*s\r\n", KB, uri);
        if ((ssl = SSL_new(ctx)) == 0)           WARN("SSL_new");
        if (!SSL_set_tlsext_host_name(ssl, buf)) WARN("SSL_set_tlsext");
        if (!SSL_set_fd(ssl, sfd))               WARN("SSL_set_fd");
        if (SSL_connect(ssl) < 1)                WARN("SSL_connect");
        if (SSL_write(ssl, res, siz) < 1)        WARN("SSL_write");
        for (j=1, bp=res, siz=0; rsiz-siz-1 > 0 && j; bp[siz+=j]=0)
                j = SSL_read(ssl, res+siz, rsiz-siz-1);
        SSL_free(ssl); /* No SSL_shutdown by design */
        if (res[0] == '1') {                                         /* Query */
                siz = sprintf(buf, "%.*s?", (int)strcspn(uri, "?\0"), uri);
                printf("Query: ");
                fgets(buf+siz, KB-siz, stdin);
                goto uri;
        } else if (res[0] == '3') {                               /* Redirect */
                sprintf(buf, "%.*s", (int)strcspn(bp+3, "\r\n"), bp+3);
                goto uri;
        }
        if (!(tmp = fopen(PATH, "wb"))) err(1, "fopen(%s)", PATH);   /* Print */
        fprintf(tmp, "[0]\tgemini://%s\n\t", uri);
	bp[strcspn(bp, "\r\n")] = '\n';
        for (i=0; *bp && (siz=strcspn(bp, "\n\0")) > -1; bp+=siz+1) {
                if (!strncmp(bp, "=>", 2)) { /* It's-a Mee, URIoo! */
                        siz = strcspn((bp += 2+strspn(bp+2, " \t")), " \t\n\0");
                        fprintf(tmp, "[%d]\t%.*s\n\t", ++i, siz, bp);
                        siz += strspn(bp+siz, " \t")-1;
                } else fprintf(tmp, "%.*s\n", siz, bp);
        }
        if (fclose(tmp) == EOF) err(1, "fclose(%s)", PATH);
        fprintf(history, "%s\n", uri);
        sprintf(buf, "%.128s %s", argc > 1 ? argv[1] : "less -XI", PATH);
        system(buf); /* Show response in pager */
        goto start;
quit:   if (fclose(history) == EOF) err(1, "fclose(history)");
        return 0;
}
