#include <stdio.h>    /* Gemini CLI protocol client written in 100 lines of C */
#include <string.h>   /* v1.0 https://github.com/ir33k/gmi100 by irek@gabr.pl */
#include <unistd.h>   /* This is free and unencumbered software released into */
#include <netdb.h>    /* the public domain.  Read more: https://unlicense.org */
#include <openssl/ssl.h>

#define KB 1024
#define ERR(msg)  do { fputs("ERROR: "msg"\n",   stderr); return 1;   } while(0)
#define WARN(msg) do { fputs("WARNING: "msg"\n", stderr); goto start; } while(0)

int main(void) {
        char uri[KB+1], tmp[KB+1], *buf, *bp, *next, *PROT="gemini://";
        int i, j, siz, sfd, err, bsiz, back, PLEN=strlen(PROT), maxw=71;
        FILE *fp; /* History file */
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;
        buf = malloc((bsiz = 2*sysconf(_SC_PAGESIZE)));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1965); /* Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method())))  ERR("SSL_CTX_new");
        if (!(fp = fopen(".gmi100", "a+b")))            ERR("fopen(.gmi100)");
        fseek(fp, 0, SEEK_END);
        back = ftell(fp)-1;
start:  fprintf(stderr, "gmi100> ");            /* PROMPT start main loop */
        if (!fgets(tmp, KB, stdin)) goto quit;
        if (tmp[1] == '\n') switch (tmp[0]) {   /* PROMPT commands */
        case 'q': goto quit;
        case 'b': /* Go back in history */
                while (!fseek(fp, --back, SEEK_SET) && back && fgetc(fp)!='\n');
                fgets(tmp, KB, fp);
                goto uri;
        }
        back = ftell(fp)-1; /* Reset history position */
        if ((i = atoi(tmp)) > 0) {              /* PROMPT navigation */
                for (bp = buf; i && bp; i--) bp = strstr(bp+3, "\n=>");
                if (i || !bp) goto start;
                for (bp += 3; *bp <= ' '; bp += 1);
                bp[strcspn(bp, " \t\n\0")] = 0;
                if (strstr(bp, "//")) uri[0] = 0;
                else if (bp[0] == '/') uri[strcspn(uri, "/\n\0")] = 0;
                else if (!strncmp(&bp[i], "../", 2)); /* Keep whole uri */
                else for(j = strlen(uri); j && uri[--j] != '/'; uri[j] = 0);
                sprintf(tmp, "%s%s", uri, bp);
        }                                       /* PROMPT typed URL */
uri:    i = strstr(tmp, "//") ? (strncmp(tmp, PROT, PLEN) ? 2 : PLEN) : 0;
        for (j=strlen(tmp)-1; tmp[j] <= ' '; j--); /* Trim */
        for (tmp[j+1]=0, j=0; tmp[i] && j<KB; uri[j]=0, i++) {
                if (!strncmp(&tmp[i], "../", 2)) for (i+=2; uri[--j-1] != '/';);
                else if (tmp[i] != ' ') uri[j++] = tmp[i];
                else if ((j+=3) < KB) strcat(uri, "%20");
        }
        fprintf(stderr, "@cli: request\t\"%s\"\n", uri);
        if ((sfd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) WARN("socket");
        sprintf(tmp, "%.*s", (int)strcspn(uri, "/\0"), uri);
        if ((he = gethostbyname(tmp)) == 0) WARN("Can't find hostname IP");
        for (i=0; he->h_addr_list[i]; i++) {
                addr.sin_addr.s_addr = *((unsigned long*)he->h_addr_list[i]);
                err = connect(sfd, (struct sockaddr*)&addr, sizeof(addr));
                if (!err) break; /* Success */
        }
        if (err) WARN("Failed to connect");
        siz = sprintf(buf, "%s%.*s\r\n", PROT, KB, uri);
        if ((ssl = SSL_new(ctx)) == 0)            WARN("SSL_new");
        if ((err = SSL_set_fd(ssl, sfd)) == 0)    WARN("SSL_set_fd");
        if ((err = SSL_connect(ssl)) < 0)         WARN("SSL_connect");
        if ((err = SSL_write(ssl, buf, siz)) < 1) WARN("SSL_write");
        for (bp=buf, siz=0; bsiz-siz-1 > 0 && err; bp[siz+=err]=0)
                err = SSL_read(ssl, buf+siz, bsiz-siz-1);
        next = strchr(bp, '\r');
        sprintf(tmp, "%.*s", (int)(next-bp), bp); /* Get response header */
        fprintf(stderr, "@cli: response\t\"%s\"\n", tmp);
        if (bp[0] == '1') {                                   /* Search query */
                siz = sprintf(tmp, "%.*s?", (int)strcspn(uri, "?\0"), uri);
                printf("Query: ");
                fgets(tmp+siz, KB-siz, stdin);
                goto uri;
        } else if (bp[0] == '3') {                                /* Redirect */
                for (i=0, j=3; tmp[j-1]; i++, j++) tmp[i]=tmp[j];
                goto uri;
        } for (bp=next+1, i=1; (next=strchr(bp, '\n')); bp=next+1) { /* Print */
                if (!strncmp(bp, "=>", 2)) { /* It's-a Mee, URIoooo! */
                        siz = strspn((bp+=2), " \t");
                        siz = strcspn((bp+=siz), " \t\n\0");
                        printf("[%d]\t%.*s\n\t", i++, siz, bp);
                        bp += siz + strspn(bp+siz, " \t");
                }
                for (;bp+maxw<next; bp+=maxw) printf("%.*s\\\n", maxw, bp);
                printf("%.*s\n", (int)(next-bp), bp);
        }
        if (close(sfd)) ERR("close(sfd)");
        SSL_free(ssl); /* No SSL_shutdown by design */
        fprintf(fp, "%s\n", uri);
        goto start;
quit:   SSL_CTX_free(ctx);
        if (fclose(fp) == EOF) ERR("fclose(fp)");
        return 0;
}
