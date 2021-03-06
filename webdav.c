/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: liubang <it.liubang@gmail.com>                               |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <getopt.h>
#include "php_webdav.h"

#define MAXSUB  200

zend_class_entry *webdav_ce;

static int error(char *err)
{
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", err);
	exit(EXIT_FAILURE);
}

static unsigned char *file_content(char * file_location, int *size)
{
	FILE *shell;
	unsigned char *buffer;
	shell = fopen(file_location,"rb");
	if(shell == NULL) {
		error(file_location);
	}

	fseek(shell, 0, SEEK_END);
	*size = ftell(shell);
	fseek(shell, 0, SEEK_SET);

	buffer = (unsigned char *)malloc((*size)+1);
	if (fread(buffer, 1, *size, shell) == 0) {
		error("empty file");
	}
	fclose(shell);
	return buffer;
}

static char* substring(char *ch, int pos, int length)
{
	char *pch = ch;
	char *subch = (char *)malloc(sizeof(char) * (length + 1));

	int i;
	pch = pch + pos;
	for (i = 0; i < length; i++) {
		subch[i] = *(pch++);
	}
	subch[length] = '\0';
	return subch;
}

static int write_file(char * filename, void * buf, int buf_len)
{
	FILE *fp = NULL;
	if (NULL == buf || buf_len <= 0)
		return -1;
	fp = fopen(filename, "ab");//append binary file
	if (NULL == fp)
		return -1;

	fwrite(buf, buf_len, 1, fp);

	fclose(fp);
	return 0;
}

static int make_socket(char *host_name, unsigned int port)
{
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;

	if (sock < 0) {
		fprintf(stderr, "error: failed to create socket\n");
		exit(1);
	}

	{
		sockopt_t optval = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	}

	struct hostent *host;
	host = gethostbyname(host_name);
	if(host == NULL) {
		error("Fail to gethostbyname");
	}

	addr.sin_family		= AF_INET;
	addr.sin_port		= htons(port);
	addr.sin_addr		= *((struct in_addr *)host->h_addr); //htonl(INADDR_ANY);
	memset(&addr.sin_zero,0,sizeof(addr.sin_zero));
	if (connect(sock,(struct sockaddr*)&addr,sizeof(addr)) == -1) {
		error("Fail to connect");
	}
	return sock;
}

#define sdtr(s, h)		\
	do {			\
		free(h);	\
		close(s);	\
	} while(0)


static int upload(char *host_name, char *file, char *create, char **response)
{
	int size;
	unsigned char *conteudo = file_content(file, &size);
	int msocket,recebidos;
	char buf[BUF_SIZE];
	msocket = make_socket(host_name, SOCK_PORT);
	char *put = malloc(BUF_SIZE);

	sprintf(put,					\
	        "PUT %s HTTP/1.1\r\n"			\
	        "Content-Length: %d\r\n"		\
	        "Host: %s\r\n"				\
	        "Connection: close\r\n\r\n"		\
	        , create, size, host_name);

	if (send(msocket,put,strlen(put),0) < 0) {
		sdtr(msocket, put);
		error("Fail to send header");
	}
	if (send(msocket, (void *)conteudo, size, 0) < 0) {
		sdtr(msocket, put);
		error("Fail to send content");
	}

	if ((recebidos = recv(msocket, buf, sizeof(buf),0)) > 0) {
		buf[recebidos] = '\0';
		*response = buf;
	} else {
		sdtr(msocket, put);
		error("put file faild");
	}

	sdtr(msocket, put);
	return 0;
}

static int delete(char *host_name, char *remote_file, char **response)
{
	int sock = make_socket(host_name, SOCK_PORT);
	char buf[BUF_SIZE];
	char *delete = malloc(BUF_SIZE);
	sprintf(delete,						\
	        "DELETE %s HTTP/1.1\r\n"			\
	        "Host: %s\r\n"					\
	        "Connection: close\r\n\r\n"			\
	        , remote_file, host_name);
	if (send(sock, delete, strlen(delete),0) < 0) {
		error("Fail to send header");
	}

	int recebidos;
	if ((recebidos = recv(sock, buf, sizeof(buf),0)) > 0) {
		buf[recebidos] = '\0';
		*response = buf;
	} else {
		sdtr(sock, delete);
		error("delete file faild");
	}

	sdtr(sock, delete);
	return 0;
}


static int post(char *host_name, char *path, char *post_data, char **resp)
{
	int sock = make_socket(host_name, SOCK_PORT);
	char response[BUF_SIZE];
	char *post = malloc(BUF_SIZE);
	snprintf(post, MAXSUB,
	         "POST %s HTTP/1.0\r\n"
	         "Host: %s\r\n"
	         "Content-type: application/x-www-form-urlencoded\r\n"
	         "Content-length: %d\r\n\r\n"
	         "%s", path, host_name, strlen(post_data), post_data);

	if (send(sock, post, strlen(post), 0) < 0) {
		sdtr(sock, post);
		error("Fail to make post request");
	}
	char c = '0';
	int status, flag = 0, resp_size;
	while((status = read(sock ,&c, 1)) != 0) {
		if (c == '\r' || c == '\n') {
			flag++;
		} else {
			if (flag > 0) {
				flag--;
			}
		}

		if (flag == 4) {
			break;
		}

	}

	while((resp_size = read(sock,response, BUF_SIZE)) != 0) {
		response[resp_size] = '\0';
	}
	*resp = response;
	sdtr(sock, post);
	return 0;
}

static int get(char *host_name, char *remote_file, char *target)
{
	int msocket, resp_size;
	msocket = make_socket(host_name, SOCK_PORT);
	char *get = malloc(BUF_SIZE);
	unsigned char response[BUF_SIZE];
	sprintf(get,					\
	        "GET %s HTTP/1.1\r\n" 			\
	        "Host: %s\r\n" 				\
	        "Accept-Encoding: gzip, deflate\r\n" 	\
	        "Connection: close\r\n\r\n"		\
	        , remote_file, host_name);

	if (send(msocket,get,strlen(get),0) < 0) {
		sdtr(msocket, get);
		error("Fail to send header");
	}

	char c = '0';
	int status, flag = 0;
	while((status = read(msocket ,&c, 1)) != 0) {
		if (c == '\r' || c == '\n') {
			flag++;
		} else {
			if (flag > 0) {
				flag--;
			}
		}

		if (flag == 4) {
			break;
		}

	}
	while((resp_size = read(msocket,response, BUF_SIZE)) != 0) {
		if (write_file(target, response, resp_size) == -1) {
			sdtr(msocket, get);
			error("write target file error!");
		}
	}
	sdtr(msocket, get);
	return 0;
}


PHP_METHOD(webdav, __construct)
{
	char *host;
	int host_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &host, &host_len) == FAILURE) {
		RETURN_FALSE;
	}
	zend_update_property_string(webdav_ce, getThis(), ZEND_STRL(PROPERTIES_HOST), host TSRMLS_CC);
}

const zend_function_entry webdav_methods[] = {
	PHP_ME(webdav, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(webdav, upload, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(webdav, get, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(webdav, post, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(webdav, delete, NULL, ZEND_ACC_PUBLIC)
	{
		NULL, NULL, NULL
	}
};

PHP_METHOD(webdav, upload)
{
	char *file;
	int file_len;
	char *target;
	int target_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &file, &file_len, &target, &target_len) ==  FAILURE) {
		RETURN_FALSE;
	}

	char *response, *host;
	zval *z_host_ptr;
	z_host_ptr = zend_read_property(webdav_ce, getThis(), ZEND_STRL(PROPERTIES_HOST), 0 TSRMLS_CC);
	host = Z_STRVAL_P(z_host_ptr);

	if (upload(host, file, target, &response) != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", "upload file faild!");
	}

	char *status_code = substring(response, 9, 3);
	if (strcmp(status_code, "204") == 0 || strcmp(status_code, "201") == 0) {
		RETURN_TRUE;
	}
	RETURN_STRING(status_code, 1);
}

PHP_METHOD(webdav, delete)
{
	char *remote_file;
	int remote_file_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &remote_file, &remote_file_len) == FAILURE) {
		RETURN_FALSE;
	}
	char *response, *host;
	zval *z_host_ptr;
	z_host_ptr = zend_read_property(webdav_ce, getThis(), ZEND_STRL(PROPERTIES_HOST), 0 TSRMLS_CC);
	host = Z_STRVAL_P(z_host_ptr);

	if (delete(host, remote_file, &response) != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", "delete file faild!");
	}

	char *status_code = substring(response, 9, 3);
	if (strcmp(status_code, "200") == 0 || strcmp(status_code, "201") == 0 || strcmp(status_code, "204") == 0) {
		RETURN_TRUE;
	}
	RETURN_LONG(atoi(response));
}

PHP_METHOD(webdav, get)
{
	char *remote_file, *target_file;
	int remote_file_len, target_file_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &remote_file, &remote_file_len, &target_file, &target_file_len) == FAILURE) {
		RETURN_FALSE;
	}

	zval *z_host_ptr;
	char *host;
	z_host_ptr = zend_read_property(webdav_ce, getThis(), ZEND_STRL(PROPERTIES_HOST), 0 TSRMLS_CC);
	host = Z_STRVAL_P(z_host_ptr);
	if (get(host, remote_file, target_file) != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", "get file faild!");
	}
	RETURN_TRUE;
}

PHP_METHOD(webdav, post)
{
	char *uri, *post_data;
	int uri_length;
	zval *z_post_data;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &uri, &uri_length, &z_post_data) == FAILURE) {
		RETURN_FALSE;
	}
	if (NULL != z_post_data) {
		switch (Z_TYPE_P(z_post_data)) {
		case IS_STRING:
			post_data =	Z_STRVAL_P(z_post_data);
			break;
		case IS_ARRAY:
			do {
				zval            **current;
				HashTable        *postfields;
				postfields = HASH_OF(z_post_data);
				if (!postfields) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't get HashTable in CURLOPT_POSTFIELDS");
					RETURN_FALSE;
				}

				int post_data_size = 0;
				char postval[BUF_SIZE] = "";
				for (zend_hash_internal_pointer_reset(postfields); zend_hash_get_current_data(postfields, (void **) &current) == SUCCESS; zend_hash_move_forward(postfields)) {
					char *string_key = NULL;
					uint string_key_len;
					ulong num_key;
					int numeric_key;
					zend_hash_get_current_key_ex(postfields, &string_key, &string_key_len, &num_key, 0, NULL);
					if (!string_key) {
						spprintf(&string_key, 0, "%ld", num_key);
						string_key_len = strlen(string_key) + 1;
					} else {
						numeric_key = 0;
					}
					SEPARATE_ZVAL(current);
					convert_to_string_ex(current);
					char *val = Z_STRVAL_PP(current);
					strcat(postval, string_key);
					strcat(postval, "=");
					strcat(postval, val);
					strcat(postval, "&");
				}
				post_data_size = strlen(postval) - 1;
				post_data = postval;
				post_data[post_data_size] = '\0';
				break;
			} while(0);
		}
	}

	zval *z_host_ptr;
	char *host, *response;
	z_host_ptr = zend_read_property(webdav_ce, getThis(), ZEND_STRL(PROPERTIES_HOST), 0 TSRMLS_CC);
	host = Z_STRVAL_P(z_host_ptr);
	if (post(host, uri, post_data, &response) != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", "post faild!");
	}
	RETURN_STRING(response, 1);
}

PHP_MINIT_FUNCTION(webdav)
{
	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "Webdav", webdav_methods);
	webdav_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_null(webdav_ce, ZEND_STRL(PROPERTIES_HOST), ZEND_ACC_PROTECTED TSRMLS_CC);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(webdav)
{
	return SUCCESS;
}

PHP_RINIT_FUNCTION(webdav)
{
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(webdav)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(webdav)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "webdav support", "enabled");
	php_info_print_table_row(2, "Version", PHP_WEBDAV_VERSION);
	php_info_print_table_row(2, "Author", PHP_WEBDAV_AUTHOR);
	php_info_print_table_end();
}


zend_module_entry webdav_module_entry = {
	STANDARD_MODULE_HEADER,
	"webdav",
	NULL,
	PHP_MINIT(webdav),
	PHP_MSHUTDOWN(webdav),
	PHP_RINIT(webdav),
	PHP_RSHUTDOWN(webdav),
	PHP_MINFO(webdav),
	PHP_WEBDAV_VERSION,
	STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_WEBDAV
ZEND_GET_MODULE(webdav)
#endif
