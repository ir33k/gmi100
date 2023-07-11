#include <stdio.h>                   /* Gemini CLI protocol client written in 100 lines of ANSI C */
#include <unistd.h>                  /* v1.0 from https://github.com/ir33k/gmi100 by irek@gabr.pl */
#include <string.h>
#include <netdb.h>
#include <openssl/ssl.h>
#define KB 1024
#define ERR(msg)  do { fputs("ERROR: "msg"\n", stderr); return 1; } while(0)
#define WARN(msg) do { fputs("WARNING: "msg"\n", stderr); goto start; } while(0)

int main(void) {
        char uri[KB+1], tmp[KB+1], *buf, *bp, *next, *PROT="gemini://";
        int i, j, siz, sfd, err, bsiz, history, PLEN=strlen(PROT), maxw=71;
        FILE *fp; /* History file pointer */
        struct hostent *he;
        struct sockaddr_in addr;
        SSL_CTX *ctx;
        SSL *ssl;
        buf = malloc((bsiz = 2*sysconf(_SC_PAGESIZE)));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1965); /* Hardcoded Gemini port */
        SSL_library_init();
        if (!(ctx = SSL_CTX_new(TLS_client_method()))) ERR("SSL_CTX_new failed");
        if (!(fp = fopen(".gmi100", "a+b"))) ERR("Can't open .gmi100 history file");
        fseek(fp, 0, SEEK_END);
        history = ftell(fp)-1;
start:  printf("gmi100> ");                     /* PROMPT start main loop */
        if (!fgets(tmp, KB, stdin)) goto quit;
        if (tmp[1] == '\n') switch (tmp[0]) {   /* PROMPT handle commands */
                case 'q': goto quit;
                case 'b': /* Go back in history */
                        while (!fseek(fp, --history, SEEK_SET) && history && fgetc(fp) != '\n');
                        fgets(tmp, KB, fp);
                        goto uri;
        }
        history = ftell(fp)-1; /* Reset history position */
        if ((i = atoi(tmp)) > 0) {             /* PROMPT handle URI navigation */
                for (bp = buf; i && bp; i--) bp = strstr(bp+3, "\n=>");
                if (i || !bp) goto start;
                for (bp += 3; *bp <= ' '; bp += 1);
                bp[strcspn(bp, " \t\n\0")] = 0;
                if (strstr(bp, "//")) uri[0] = 0;
                else if (bp[0] == '/') uri[strcspn(uri, "/\n\0")] = 0;
                else if (!strncmp(&bp[i], "../", 2)); /* Keep whole uri */
                else for(j = strlen(uri); j && uri[--j] != '/'; uri[j] = 0);
                sprintf(tmp, "%s%s", uri, bp);
        }                                       /* PROMPT else handle URL typed by hand */
uri:    i = strstr(tmp, "//") ? (strncmp(tmp, PROT, PLEN) ? 2 : PLEN) : 0; /* Remove protocol */
        for (j=strlen(tmp)-1; tmp[j] <= ' '; j--); /* Trim */
        for (tmp[j+1]=0, j=0; tmp[i] && j<KB; uri[j]=0, i++) { /* Normalize URI */
                if (!strncmp(&tmp[i], "../", 2)) for (j--, i+=2; uri[j-1] != '/'; j--);
                else if (tmp[i] != ' ') uri[j++] = tmp[i];
                else if ((j+=3) < KB) strcat(uri, "%20");
        }
        fprintf(stderr, "@cli: request\t\"%s\"\n", uri);
        if ((sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) WARN("Failed to open socket");
        sprintf(tmp, "%.*s", (int)strcspn(uri, "/\0"), uri);
        if ((he = gethostbyname(tmp)) == 0) WARN("Can't find IP for given host");
        for (i=0; he->h_addr_list[i]; i++) {
                addr.sin_addr.s_addr = *((unsigned long*)he->h_addr_list[i]);
                if (!(err = connect(sfd, (struct sockaddr*)&addr, sizeof(addr)))) break; /* Ok */
        }
        if (err) WARN("Failed to connect");
        siz = sprintf(buf, "%s%.*s\r\n", PROT, KB, uri);
        if ((ssl = SSL_new(ctx)) == 0)             WARN("SSL_new");
        if ((err = SSL_set_fd(ssl, sfd)) == 0)     WARN("SSL_set_fd");
        if ((err = SSL_connect(ssl)) < 0)          WARN("SSL_connect");
        if ((err = SSL_write(ssl, buf, siz)) <= 0) WARN("SSL_write");
        for (siz=0; bsiz-siz-1 > 0 && err; siz+=err) err = SSL_read(ssl, buf+siz, bsiz-siz-1);
        buf[siz] = 0;
        bp = buf;
        next = strchr(bp, '\r');
        sprintf(tmp, "%.*s", (int)(next-bp), bp); /* Get response header */
        bp = next+1;
        fprintf(stderr, "@cli: response\t\"%s\"\n", tmp);
        /* TODO(irek): I should handle somehow other content types than gemin/text */
        /* TODO(irek): I'm not pasing gemini.thebackupbox.net/test/./torture/0011 */
        if (buf[0] == '1') {                                  /* RESPONSE prompt for query */
                siz = sprintf(tmp, "%.*s?", (int)strcspn(uri, "?\0"), uri);
                printf("Query: ");
                fgets(tmp+siz, KB-siz, stdin); /* Get search query from user */
                goto uri;
        } else if (buf[0] == '3') {                           /* RESPONSE redirect */
                for (i=0, j=3; tmp[j-1]; i++, j++) tmp[i]=tmp[j];
                goto uri;
        } for (i=1; (next = strchr(bp, '\n')); bp = next+1) { /* RESPONSE print content */
                if (!strncmp(bp, "=>", 2)) { /* It's-a Mee, URIoooo! */
                        siz = strspn((bp+=2), " \t");
                        siz = strcspn((bp+=siz), " \t\n\0");
                        printf("[%d]\t%.*s\n\t", i++, siz, bp);
                        bp += siz + strspn(bp+siz, " \t");
                }
                for (;bp+maxw<next; bp+=maxw) printf("%.*s\\\n", maxw, bp);
                printf("%.*s\n", (int)(next-bp), bp);
        }
        if (close(sfd)) ERR("Failed to close socket");
        SSL_free(ssl); /* No SSL_shutdown by design */
        fprintf(fp, "%s\n", uri);
        goto start;
quit:   SSL_CTX_free(ctx);
        if (fclose(fp) == EOF) ERR("Failed to close .gmi100 history file");
        return 0;
} /* This is free and unencumbered software released into the public domain https://unlicense.org */
