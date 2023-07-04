#include <stdio.h>                   /* Gemini CLI protocol client written in 100 lines of ANSI C */
#include <unistd.h>                  /* v1.0 from https://github.com/ir33k/gmi100 by irek@gabr.pl */
#include <string.h>
#include <netdb.h>
#include <openssl/ssl.h>

#define KB 1024
#define ERR(msg) do { fputs("ERR: "msg"\n", stderr); goto start; } while(0)

int main(void) {
        char uri[KB+1], tmp[KB+1], *buf, *bp, *next, *PROT="gemini://";
        int i, j, siz, sfd, err, bsiz, PLEN=strlen(PROT);
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;
        bsiz = 2*sysconf(_SC_PAGESIZE);
        buf = malloc(bsiz);     /* There is no free(), OS will do that for us */
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1965); /* Hardcoded Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) {
                fprintf(stderr, "ERR: SSL_CTX_new failed");
                return 1;
        }
start:  printf("gmi100> ");                     /* PROMPT start main loop */
        if (!fgets(tmp, KB, stdin)) goto quit;
        if (tmp[1] == '\n') switch (tmp[0]) {   /* PROMPT handle commands */
                case 'q': goto quit;
                case 'b': goto start; /* TODO(irek): Implement history */
        } if ((i = atoi(tmp)) > 0) {            /* PROMPT handle URI navigation */
                for (bp = buf; i && bp; i--) bp = strstr(bp+3, "\n=>");
                if (i || !bp) goto start;
                for (bp += 3; *bp <= ' '; bp += 1);
                bp[strcspn(bp, " \t\n\0")] = 0;
                if (!strncmp(bp, PROT, PLEN)) strcpy(tmp, bp);
                else if (bp[0] == '/') sprintf(tmp, "%.*s%s", (int)strcspn(uri, "/\0"), uri, bp);
                else sprintf(tmp, "%s%s", uri, bp);
        }                                       /* PROMPT else handle URL typed by hand */
uri:    i = strncmp(tmp, PROT, PLEN) ? 0 : PLEN;   /* Skip protocol if provided */
        for (j=0; tmp[i] && j<KB; uri[j]=0, i++) { /* Normalize URI */
                /* TODO(irek): Hande "/" and relative paths correctly */
                if (!strncmp(&tmp[i], "/..", 3)) for (j--, i+=2; uri[j] != '/'; j--);
                else if (tmp[i] == '\n') continue;
                else if (tmp[i] != ' ') uri[j++] = tmp[i];
                else if ((j+=3) < KB) strcat(uri, "%20");
        }
        fprintf(stderr, "@cli: request\t\"%s\"\n", uri);
        if ((sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) ERR("Failed to open socket");
        sprintf(tmp, "%.*s", (int)strcspn(uri, "/\0"), uri);
        if ((he = gethostbyname(tmp)) == 0) ERR("Can't find IP for given host");
        for (i=0; he->h_addr_list[i]; i++) {
                addr.sin_addr.s_addr = *((unsigned long*)he->h_addr_list[i]);
                if (!(err = connect(sfd, (struct sockaddr*)&addr, sizeof(addr)))) break; /* Ok */
        }
        if (err) ERR("Failed to connect");
        siz = sprintf(buf, "%s%.*s\r\n", PROT, KB, uri);
        if ((ssl = SSL_new(ctx)) == 0)             ERR("SSL_new");
        if ((err = SSL_set_fd(ssl, sfd)) == 0)     ERR("SSL_set_fd");
        if ((err = SSL_connect(ssl)) < 0)          ERR("SSL_connect");
        if ((err = SSL_write(ssl, buf, siz)) <= 0) ERR("SSL_write");
        for (siz=0; bsiz-siz-1 > 0 && err; siz+=err) /* Read entire server response */
                err = SSL_read(ssl, buf+siz, bsiz-siz-1); 
        buf[siz] = 0;
        bp = buf;
        next = strchr(bp, '\r');
        sprintf(tmp, "%.*s", (int)(next-bp), bp); /* Get response header */
        bp = next+1;
        fprintf(stderr, "@cli: response\t\"%s\"\n", tmp);
        if (buf[0] == '1') {                                  /* RESPONSE prompt for query */
                siz = sprintf(tmp, "%.*s?", (int)strcspn(uri, "?\0"), uri);
                printf("Query: ");
                fgets(tmp+siz, KB-siz, stdin); /* Get search query from user */
                goto uri;
        } else if (buf[0] == '3') {                           /* RESPONSE redirect */
                for (i=0, j=3, siz=strlen(tmp); j<=siz; i++, j++) tmp[i]=tmp[j];
                goto uri;
        } for (i=1; (next = strchr(bp, '\n')); bp = next+1) { /* RESPONSE print content */
                if (!strncmp(bp, "=>", 2)) printf("[%d]\t", i++); /* It's-a Mee, URIoooo! */
                printf("%.*s\n", (int)(next-bp), bp);
        }
        close(sfd);
        SSL_free(ssl);          /* No SSL_shutdown by design */
        goto start;
quit:   SSL_CTX_free(ctx);
        return 0;
} /* This is free and unencumbered software released into the public domain https://unlicense.org */
