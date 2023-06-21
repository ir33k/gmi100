#include <netdb.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <unistd.h>

#define URISIZ 1024
#define PORT 1965		/* Default Gemini port */

enum { ERR_OK=0, ERR_SFD, ERR_HOST, ERR_TCP, ERR_SSL, ERR_CONN,
	ERR_WRITE, _ERR_SIZ };
static const char *s_err[_ERR_SIZ] = {
	[ERR_OK]    = "ok, no errors",
	[ERR_SFD]   = "failed to open socket",
	[ERR_HOST]  = "invalid host (domain) name",
	[ERR_TCP]   = "failed to establish tcp connection",
	[ERR_SSL]   = "failed to create ssl",
	[ERR_CONN]  = "failed to establish ssl connection",
	[ERR_WRITE] = "failed to send message to server"
};
int tcp(int *res, char *host) {
	struct hostent *he;
	struct sockaddr_in addr;
	if ((*res = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) return ERR_SFD;
	if (!(he = gethostbyname(host))) return ERR_HOST;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	for (int i = 0; he->h_addr_list[i]; i++) {
		addr.sin_addr.s_addr = *((unsigned long *)he->h_addr_list[i]);
		if (!connect(*res, (struct sockaddr *)&addr, sizeof(addr))) return ERR_OK;
	}
	return ERR_TCP;
}
int ssl_connect(SSL **res, SSL_CTX *ctx, int sfd) {
	if (!(*res = SSL_new(ctx))) return ERR_SSL;
	SSL_set_fd(*res, sfd);
	if (SSL_connect(*res) == -1) return ERR_CONN;
	return ERR_OK;
}
int main(void) {
	char uri[URISIZ+1], buf[BUFSIZ];
	int err, siz, sfd;
	SSL_CTX *ssl_ctx;
	SSL *ssl;
	SSL_library_init();
	if (!(ssl_ctx = SSL_CTX_new(TLS_client_method()))) {
		fprintf(stderr, "SSL_CTX_new");
		return 1;
	}
        while(scanf("%1024s", uri) != EOF) {
		if ((err = tcp(&sfd, uri))) goto err;
		if ((err = ssl_connect(&ssl, ssl_ctx, sfd))) goto err;
		siz = sprintf(buf, "gemini://%s:%d\r\n", uri, PORT);
		if (SSL_write(ssl, buf, siz) == 0) {
			err = ERR_WRITE;
			goto err;
		}
		while ((siz = SSL_read(ssl, buf, BUFSIZ)) > 0) {
			fwrite(buf, 1, siz, stdout);
		}
		close(sfd);
		SSL_free(ssl); /* SSL_shutdown skipped.  Good idea? */
		continue;
	err:
		fprintf(stderr, "ERROR: %s\n", s_err[err]);
	}
	SSL_CTX_free(ssl_ctx);
	return 0;
}
