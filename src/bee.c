#include <time.h>
#include <stdio.h>
#include <limits.h>

#include "php_bee.h"

#include "bee_network.h"
#include "bee_msgpack.h"
#include "bee_proto.h"
#include "bee_schema.h"
#include "bee_tp.h"

int __bee_authenticate(bee_connection *obj);

double
now_gettimeofday(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1e9 + t.tv_usec * 1e3;
}

ZEND_DECLARE_MODULE_GLOBALS(bee)

static int le_bee = 0;
static zend_class_entry *bee_ce = NULL;
static zend_class_entry *bee_exception_ce = NULL;

#define BEE_PARSE_PARAMS(ID, FORMAT, ...)				\
	zval *ID;							\
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC,	\
				getThis(), "O" FORMAT,			\
				&ID, Bee_ptr,		\
				__VA_ARGS__) == FAILURE)		\
		RETURN_FALSE;

#define BEE_FETCH_OBJECT(NAME, ID)				\
	bee_object *t_##NAME = (bee_object *)		\
			zend_object_store_get_object(ID TSRMLS_CC);	\
	bee_connection *NAME = (t_##NAME)->obj;

#define BEE_CONNECT_ON_DEMAND(CON, ID)					\
	if (!CON->stream) {							\
		if (__bee_connect(t_##CON, ID TSRMLS_CC) == FAILURE)	\
			RETURN_FALSE;						\
	}									\
	if (CON->stream && php_stream_eof(CON->stream) != 0)			\
		if (__bee_reconnect(t_##CON, ID TSRMLS_CC) == FAILURE)	\
			RETURN_FALSE;

#define BEE_RETURN_DATA(HT, HEAD, BODY)				\
	HashTable *ht_ ## HT = HASH_OF(HT);				\
	zval **answer = NULL;						\
	if (zend_hash_index_find(ht_ ## HT, BEEX_DATA,			\
			(void **)&answer) == FAILURE || !answer) {	\
		THROW_EXC("No field DATA in body");			\
		zval_ptr_dtor(&HEAD);					\
		zval_ptr_dtor(&BODY);					\
		RETURN_FALSE;						\
	}								\
	RETVAL_ZVAL(*answer, 1, 0);					\
	zval_ptr_dtor(&HEAD);						\
	zval_ptr_dtor(&BODY);						\
	return;

#define BEE_PERSISTENT_FIND(NAME, LEN, WHERE)			\
	zend_hash_find(&EG(persistent_list), (NAME), (LEN),		\
		       (void *)&(WHERE))

#define BEE_PERSISTENT_UPDATE(NAME, WHERE)			\
	zend_hash_find(&EG(persistent_list), (NAME), strlen((NAME)),	\
		       (void *)&(WHERE))

#if HAVE_SPL
static zend_class_entry *spl_ce_RuntimeException = NULL;
#endif

PHP_BEE_API
zend_class_entry *php_bee_get_exception_base(int root TSRMLS_DC)
{
#if HAVE_SPL
	if (!root) {
		if (!spl_ce_RuntimeException) {
			zend_class_entry **pce;

			if (zend_hash_find(CG(class_table), "runtimeexception",
					   sizeof("RuntimeException"),
					   (void **) &pce) == SUCCESS) {
				spl_ce_RuntimeException = *pce;
				return *pce;
			}
		} else {
			return spl_ce_RuntimeException;
		}
	}
#endif
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default(TSRMLS_C);
#endif
}

zend_function_entry bee_module_functions[] = {
	{NULL, NULL, NULL}
};

zend_module_entry bee_module_entry = {
	STANDARD_MODULE_HEADER,
	PHP_BEE_EXTNAME,
	bee_module_functions,
	PHP_MINIT(bee),
	PHP_MSHUTDOWN(bee),
	PHP_RINIT(bee),
	NULL,
	PHP_MINFO(bee),
	PHP_BEE_VERSION,
	STANDARD_MODULE_PROPERTIES
};

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("bee.persistent", "0", PHP_INI_ALL,
			  OnUpdateBool, persistent, zend_bee_globals,
			  bee_globals)
	STD_PHP_INI_ENTRY("bee.timeout", "10.0", PHP_INI_ALL,
			  OnUpdateReal, timeout, zend_bee_globals,
			  bee_globals)
	STD_PHP_INI_ENTRY("bee.request_timeout", "10.0", PHP_INI_ALL,
			  OnUpdateReal, request_timeout, zend_bee_globals,
			  bee_globals)
	STD_PHP_INI_ENTRY("bee.retry_count", "1", PHP_INI_ALL,
			  OnUpdateLong, retry_count, zend_bee_globals,
			  bee_globals)
	STD_PHP_INI_ENTRY("bee.retry_sleep", "0.1", PHP_INI_ALL,
			  OnUpdateReal, retry_sleep, zend_bee_globals,
			  bee_globals)
PHP_INI_END()

#ifdef COMPILE_DL_BEE
ZEND_GET_MODULE(bee)
#endif

static int
bee_stream_send(bee_connection *obj TSRMLS_DC) {
	int rv = beexll_stream_send(obj->stream, SSTR_BEG(obj->value),
				   SSTR_LEN(obj->value));
	if (rv) return FAILURE;
	SSTR_LEN(obj->value) = 0;
	smart_string_nullify(obj->value);
	return SUCCESS;
}

static char *persistent_id(const char *host, int port, const char *login,
			   const char *prefix, int *olen,
			   const char *suffix, int suffix_len) {
	char *plist_id = NULL, *tmp = NULL;
	/* if login is not defined, then login is 'guest' */
	login = (login ? login : "guest");
	int len = 0;
	len = spprintf(&plist_id, 0, "bee-%s:id=%s:%d-%s", prefix, host,
		       port, login) + 1;
	if (suffix) {
		len = spprintf(&tmp,0,"%s[%.*s]",plist_id,suffix_len,suffix);
		efree(plist_id);
		plist_id = tmp;
	}
	tmp = pestrdup(plist_id, 1);
	efree(plist_id);
	if (olen)
		*olen = len;
	return tmp;
}

/*
 * Legacy rtsisyk code, php_stream_read made right
 * See https://bugs.launchpad.net/bee/+bug/1182474
 */
static size_t
bee_stream_read(bee_connection *obj, char *buf, size_t size) {
	return beexll_stream_read2(obj->stream, buf, size);
}

static void
bee_stream_close(bee_connection *obj) {
	if (obj->stream || obj->persistent_id) {
		beexll_stream_close(obj->stream, obj->persistent_id);
	}
	obj->stream = NULL;
	if (obj->persistent_id != NULL) {
		pefree(obj->persistent_id, 1);
		obj->persistent_id = NULL;
	}
}

int __bee_connect(bee_object *t_obj, zval *id TSRMLS_DC) {
	bee_connection *obj = t_obj->obj;
	int status = SUCCESS;
	long count = BEE_G(retry_count);
	struct timespec sleep_time = {0};
	double_to_ts(INI_FLT("retry_sleep"), &sleep_time);
	char *err = NULL;

	if (t_obj->is_persistent) {
		if (!obj->persistent_id)
			obj->persistent_id = persistent_id(obj->host, obj->port,
							   obj->orig_login,
							   "stream", NULL,
							   obj->suffix,
							   obj->suffix_len);
		int rv = beexll_stream_fpid2(obj->persistent_id, &obj->stream);
		if (obj->stream == NULL || rv != PHP_STREAM_PERSISTENT_SUCCESS)
			goto retry;
		return status;
	}
retry:
	while (count > 0) {
		--count;
		if (err) {
			/* If we're here, then there war error */
			nanosleep(&sleep_time, NULL);
			efree(err);
			err = NULL;
		}
		if (t_obj->is_persistent) {
			if (obj->persistent_id)
				pefree(obj->persistent_id, 1);
			obj->persistent_id = persistent_id(obj->host, obj->port,
							   obj->orig_login,
							   "stream", NULL,
							   obj->suffix,
							   obj->suffix_len);

		}
		if (beexll_stream_open(obj->host, obj->port,
				      obj->persistent_id,
				      &obj->stream, &err) == -1) {
			continue;
		}
		if (beexll_stream_read2(obj->stream, obj->greeting,
				       GREETING_SIZE) == -1) {
			continue;
		}
		++count;
		break;
	}
	if (count == 0) {
		char errstr[256];
		snprintf(errstr, 256, "%s", err);
		THROW_EXC(errstr);
		efree(err);
		return FAILURE;
	}
	if (obj->login != NULL && obj->passwd != NULL) {
		status = __bee_authenticate(obj);
	}
	return status;
}

int __bee_reconnect(bee_object *t_obj, zval *id TSRMLS_DC) {
	bee_connection *obj = t_obj->obj;
	bee_stream_close(obj);
	return __bee_connect(t_obj, id TSRMLS_CC);
}

static void
bee_connection_free(bee_connection *obj, int is_persistent
			  TSRMLS_DC) {
	if (obj == NULL)
		return;
	if (obj->greeting) {
		pefree(obj->greeting, is_persistent);
		obj->greeting = NULL;
	}
	bee_stream_close(obj);
	if (obj->persistent_id) {
		pefree(obj->persistent_id, 1);
		obj->persistent_id = NULL;
	}
	if (obj->schema) {
		bee_schema_delete(obj->schema, is_persistent);
		obj->schema = NULL;
	}
	if (obj->host) {
		pefree(obj->host, is_persistent);
		obj->host = NULL;
	}
	if (obj->login) {
		pefree(obj->login, is_persistent);
		obj->login = NULL;
	}
	if (obj->orig_login) {
		pefree(obj->orig_login, is_persistent);
		obj->orig_login = NULL;
	}
	if (obj->suffix) {
		pefree(obj->suffix, is_persistent);
		obj->suffix = NULL;
	}
	if (obj->passwd) {
		pefree(obj->passwd, is_persistent);
		obj->passwd = NULL;
	}
	if (obj->value) {
		smart_string_free_ex(obj->value, 1);
		pefree(obj->value, 1);
		obj->value = NULL;
	}
	if (obj->tps) {
		bee_tp_free(obj->tps, is_persistent);
		obj->tps = NULL;
	}
	pefree(obj, is_persistent);
}

static void
bee_object_free(bee_object *obj TSRMLS_DC) {
	if (obj == NULL)
		return;
	if (!obj->is_persistent && obj->obj != NULL) {
		bee_connection_free(obj->obj, obj->is_persistent
					  TSRMLS_CC);
		obj->obj = NULL;
	}
	efree(obj);
}

static zend_object_value bee_create(zend_class_entry *ce TSRMLS_DC) {
	zend_object_value retval;
	bee_object *obj = NULL;

	obj = (bee_object *)ecalloc(1, sizeof(bee_object));
	zend_object_std_init(&obj->zo, ce TSRMLS_CC);
#if PHP_VERSION_ID >= 50400
	object_properties_init((zend_object *)obj, ce);
#else
	{
		zval *tmp;
		zend_hash_copy(obj->zo.properties, &ce->default_properties,
				(copy_ctor_func_t) zval_add_ref, (void *)&tmp,
				sizeof(zval *));
	}
#endif
	retval.handle = zend_objects_store_put(obj,
		(zend_objects_store_dtor_t )zend_objects_destroy_object,
		(zend_objects_free_object_storage_t )bee_object_free,
			NULL TSRMLS_CC);
	retval.handlers = zend_get_std_object_handlers();
	return retval;
}

static int64_t bee_step_recv(bee_connection *obj, unsigned long sync,
				   zval **header, zval **body TSRMLS_DC) {
	char pack_len[5] = {0, 0, 0, 0, 0};
	*header = NULL;
	*body = NULL;
	if (bee_stream_read(obj, pack_len, 5) != 5) {
		THROW_EXC("Can't read query from server (failed to read length)");
		goto error_con;
	}
	if (php_mp_check(pack_len, 5)) {
		THROW_EXC("Failed verifying msgpack");
		goto error_con;
	}
	size_t body_size = php_mp_unpack_package_size(pack_len);
	smart_string_ensure(obj->value, body_size);
	if (bee_stream_read(obj, SSTR_POS(obj->value),
				  body_size) != body_size) {
		THROW_EXC("Can't read query from server (failed to read %d "
			  "bytes from server [header + body])", body_size);
		goto error;
	}
	SSTR_LEN(obj->value) += body_size;

	char *pos = SSTR_BEG(obj->value);
	if (php_mp_check(pos, body_size)) {
		THROW_EXC("Failed verifying header [bad msgpack]");
		goto error;
	}
	if (php_mp_unpack(header, &pos) == FAILURE ||
	    Z_TYPE_PP(header) != IS_ARRAY) {
		*header = NULL;
		goto error;
	}
	if (php_mp_check(pos, body_size)) {
		THROW_EXC("Failed verifying body [bad msgpack]");
		goto error_con;
	}
	if (php_mp_unpack(body, &pos) == FAILURE) {
		*body = NULL;
		goto error;
	}

	HashTable *hash = HASH_OF(*header);
	zval **val = NULL;

	if (zend_hash_index_find(hash, BEEX_SYNC, (void **)&val) == SUCCESS) {
		if (Z_LVAL_PP(val) != sync) {
			THROW_EXC("request sync is not equal response sync. "
				  "closing connection");
			goto error_con;
		}
	}
	val = NULL;
	if (zend_hash_index_find(hash, BEEX_CODE, (void **)&val) == SUCCESS) {
		if (Z_LVAL_PP(val) == BEEX_OK) {
			SSTR_LEN(obj->value) = 0;
			smart_string_nullify(obj->value);
			return SUCCESS;
		}
		HashTable *hash = HASH_OF(*body);
		zval **errstr = NULL;
		long errcode = Z_LVAL_PP(val) & ((1 << 15) - 1 );
		if (zend_hash_index_find(hash, BEEX_ERROR, (void **)&errstr) == FAILURE) {
			ALLOC_INIT_ZVAL(*errstr);
			ZVAL_STRING(*errstr, "empty", 1);
		}
		THROW_EXC("Query error %d: %s", errcode, Z_STRVAL_PP(errstr),
			  Z_STRLEN_PP(errstr));
		goto error;
	}
	THROW_EXC("Failed to retrieve answer code");
error_con:
	bee_stream_close(obj);
	obj->stream = NULL;
error:
	if (*header) zval_ptr_dtor(header);
	if (*body) zval_ptr_dtor(body);
	SSTR_LEN(obj->value) = 0;
	smart_string_nullify(obj->value);
	return FAILURE;
}

// connect, reconnect, flush_schema, close, ping
ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, port)
	ZEND_ARG_INFO(0, login)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, persistent_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_authenticate, 0, 0, 1)
	ZEND_ARG_INFO(0, login)
	ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_select, 0, 0, 1)
	ZEND_ARG_INFO(0, space)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, index)
	ZEND_ARG_INFO(0, limit)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, iterator)
ZEND_END_ARG_INFO()

// insert, replace
ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_space_tuple, 0, 0, 2)
	ZEND_ARG_INFO(0, space)
	ZEND_ARG_ARRAY_INFO(0, tuple, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_delete, 0, 0, 2)
	ZEND_ARG_INFO(0, space)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, index)
ZEND_END_ARG_INFO()

// call, eval
ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_proc_tuple, 0, 0, 1)
	ZEND_ARG_INFO(0, proc)
	ZEND_ARG_INFO(0, tuple)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_update, 0, 0, 3)
	ZEND_ARG_INFO(0, space)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_ARRAY_INFO(0, args, 0)
	ZEND_ARG_INFO(0, index)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bee_upsert, 0, 0, 3)
	ZEND_ARG_INFO(0, space)
	ZEND_ARG_ARRAY_INFO(0, tuple, 0)
	ZEND_ARG_ARRAY_INFO(0, args, 0)
ZEND_END_ARG_INFO()

#define BEEX_MEP(name, args) PHP_ME(Bee, name, args, ZEND_ACC_PUBLIC)
#define BEEX_MAP(alias, name, args) PHP_MALIAS(Bee, alias, name, args, ZEND_ACC_PUBLIC)
const zend_function_entry Bee_methods[] = {
	BEEX_MEP(__construct,  arginfo_bee_construct)
	BEEX_MEP(connect,      arginfo_bee_void)
	BEEX_MEP(reconnect,    arginfo_bee_void)
	BEEX_MEP(close,        arginfo_bee_void)
	BEEX_MEP(flush_schema, arginfo_bee_void)
	BEEX_MEP(authenticate, arginfo_bee_authenticate)
	BEEX_MEP(ping,         arginfo_bee_void)
	BEEX_MEP(select,       arginfo_bee_select)
	BEEX_MEP(insert,       arginfo_bee_space_tuple)
	BEEX_MEP(replace,      arginfo_bee_space_tuple)
	BEEX_MEP(call,         arginfo_bee_proc_tuple)
	BEEX_MEP(eval,         arginfo_bee_proc_tuple)
	BEEX_MEP(delete,       arginfo_bee_delete)
	BEEX_MEP(update,       arginfo_bee_update)
	BEEX_MEP(upsert,       arginfo_bee_upsert)
	BEEX_MAP(evaluate,     eval,         arginfo_bee_proc_tuple)
	BEEX_MAP(flushSchema,  flush_schema, arginfo_bee_void)
	BEEX_MAP(disconnect,   close,        arginfo_bee_void)
	{NULL, NULL, NULL}
};
#undef BEEX_MEP
#undef BEEX_MAP

/* ####################### HELPERS ####################### */

zval *pack_key(zval *args, char select) {
	if (args && Z_TYPE_P(args) == IS_ARRAY)
		return args;
	zval *arr = NULL;
	ALLOC_INIT_ZVAL(arr);
	if (select && (!args || Z_TYPE_P(args) == IS_NULL)) {
		array_init_size(arr, 0);
		return arr;
	}
	array_init_size(arr, 1);
	Z_ADDREF_P(args);
	add_next_index_zval(arr, args);
	return arr;
}

int convert_iterator(zval *iter, int all) {
	TSRMLS_FETCH();
	if (iter == NULL || Z_TYPE_P(iter) == IS_NULL) {
		if (all) {
			return ITERATOR_ALL;
		} else {
			return ITERATOR_EQ;
		}
	} else if (Z_TYPE_P(iter) == IS_LONG) {
		return Z_LVAL_P(iter);
	} else if (Z_TYPE_P(iter) != IS_STRING) {
		THROW_EXC("Bad iterator type, expected null/String/Long, got"
			   "%s", op_to_string(Z_TYPE_P(iter)));
	}
	const char *i = Z_STRVAL_P(iter);
	size_t i_len = Z_STRLEN_P(iter);
	int first = toupper(i[0]);
	switch (first) {
	case 'A':
		if (i_len == 3 && toupper(i[1]) == 'L' && toupper(i[2]) == 'L')
			return ITERATOR_ALL;
		break;
	case 'B':
		if (i_len > 7            && toupper(i[1]) == 'I' &&
		    toupper(i[2]) == 'T' && toupper(i[3]) == 'S' &&
		    toupper(i[4]) == 'E' && toupper(i[5]) == 'T' &&
		    toupper(i[6]) == '_') {
			if (i_len == 18           && toupper(i[7])  == 'A' &&
			    toupper(i[8])  == 'L' && toupper(i[9])  == 'L' &&
			    toupper(i[10]) == '_' && toupper(i[11]) == 'N' &&
			    toupper(i[12]) == 'O' && toupper(i[13]) == 'T' &&
			    toupper(i[14]) == '_' && toupper(i[15]) == 'S' &&
			    toupper(i[16]) == 'E' && toupper(i[17]) == 'T')
				return ITERATOR_BITSET_ALL_NOT_SET;
			else if (i_len == 14           && toupper(i[7])  == 'A' &&
			         toupper(i[8])  == 'L' && toupper(i[9])  == 'L' &&
			         toupper(i[10]) == '_' && toupper(i[11]) == 'S' &&
			         toupper(i[12]) == 'E' && toupper(i[13]) == 'T')
				return ITERATOR_BITSET_ALL_SET;
			else if (i_len == 14           && toupper(i[7])  == 'A' &&
			         toupper(i[8])  == 'N' && toupper(i[9])  == 'Y' &&
			         toupper(i[10]) == '_' && toupper(i[11]) == 'S' &&
			         toupper(i[12]) == 'E' && toupper(i[13]) == 'T')
				return ITERATOR_BITSET_ANY_SET;
		}
		if (i_len > 4            && toupper(i[1]) == 'I' &&
		    toupper(i[2]) == 'T' && toupper(i[3]) == 'S' &&
		    toupper(i[4]) == '_') {
			if (i_len == 16           && toupper(i[5])  == 'A' &&
			    toupper(i[6])  == 'L' && toupper(i[7])  == 'L' &&
			    toupper(i[8])  == '_' && toupper(i[9])  == 'N' &&
			    toupper(i[10]) == 'O' && toupper(i[11]) == 'T' &&
			    toupper(i[12]) == '_' && toupper(i[13]) == 'S' &&
			    toupper(i[14]) == 'E' && toupper(i[15]) == 'T')
				return ITERATOR_BITSET_ALL_NOT_SET;
			else if (i_len == 12           && toupper(i[5])  == 'A' &&
			         toupper(i[6])  == 'L' && toupper(i[7])  == 'L' &&
			         toupper(i[8])  == '_' && toupper(i[9])  == 'S' &&
			         toupper(i[10]) == 'E' && toupper(i[11]) == 'T')
				return ITERATOR_BITSET_ALL_SET;
			else if (i_len == 12           && toupper(i[5])  == 'A' &&
			         toupper(i[6])  == 'N' && toupper(i[7])  == 'Y' &&
			         toupper(i[8])  == '_' && toupper(i[9])  == 'S' &&
			         toupper(i[10]) == 'E' && toupper(i[11]) == 'T')
				return ITERATOR_BITSET_ANY_SET;
		}
		break;
	case 'E':
		if (i_len == 2 && toupper(i[1]) == 'Q')
			return ITERATOR_EQ;
		break;
	case 'G':
		if (i_len == 2) {
			int second = toupper(i[1]);
			switch (second) {
			case 'E':
				return ITERATOR_GE;
			case 'T':
				return ITERATOR_GT;
			}
		}
		break;
	case 'L':
		if (i_len == 2) {
			int second = toupper(i[1]);
			switch (second) {
			case 'T':
				return ITERATOR_LT;
			case 'E':
				return ITERATOR_LE;
			}
		}
		break;
	case 'N':
		if (i_len == 8           && toupper(i[1]) == 'E' &&
		    toupper(i[2]) == 'I' && toupper(i[3]) == 'G' &&
		    toupper(i[4]) == 'H' && toupper(i[5]) == 'B' &&
		    toupper(i[6]) == 'O' && toupper(i[7]) == 'R')
			return ITERATOR_NEIGHBOR;
		break;
	case 'O':
		if (i_len == 8           && toupper(i[1]) == 'V' &&
		    toupper(i[2]) == 'E' && toupper(i[3]) == 'R' &&
		    toupper(i[4]) == 'L' && toupper(i[5]) == 'A' &&
		    toupper(i[6]) == 'P' && toupper(i[7]) == 'S')
			return ITERATOR_OVERLAPS;
		break;
	case 'R':
		if (i_len == 3 && toupper(i[1]) == 'E' && toupper(i[2]) == 'Q')
			return ITERATOR_REQ;
		break;
	default:
		break;
	};
error:
	THROW_EXC("Bad iterator name '%.*s'", i_len, i);
	return -1;
}

zval *bee_update_verify_op(zval *op, long position TSRMLS_DC) {
	if (Z_TYPE_P(op) != IS_ARRAY || !php_mp_is_hash(op)) {
		THROW_EXC("Op must be MAP at pos %d", position);
		return NULL;
	}
	HashTable *ht = HASH_OF(op);
	size_t n = zend_hash_num_elements(ht);
	zval *arr; ALLOC_INIT_ZVAL(arr); array_init_size(arr, n);
	zval **opstr, **oppos;
	if (zend_hash_find(ht, "op", 3, (void **)&opstr) == FAILURE ||
			!opstr || Z_TYPE_PP(opstr) != IS_STRING ||
			Z_STRLEN_PP(opstr) != 1) {
		THROW_EXC("Field OP must be provided and must be STRING with "
				"length=1 at position %d", position);
		goto cleanup;
	}
	if (zend_hash_find(ht, "field", 6, (void **)&oppos) == FAILURE ||
			!oppos || Z_TYPE_PP(oppos) != IS_LONG) {
		THROW_EXC("Field FIELD must be provided and must be LONG at "
				"position %d", position);
		goto cleanup;
	}
	zval **oparg, **splice_len, **splice_val;
	switch(Z_STRVAL_PP(opstr)[0]) {
	case ':':
		if (n != 5) {
			THROW_EXC("Five fields must be provided for splice"
					" at position %d", position);
			goto cleanup;
		}
		if (zend_hash_find(ht, "offset", 7,
				(void **)&oparg) == FAILURE ||
				!oparg || Z_TYPE_PP(oparg) != IS_LONG) {
			THROW_EXC("Field OFFSET must be provided and must be LONG for "
					"splice at position %d", position);
			goto cleanup;
		}
		if (zend_hash_find(ht, "length", 7, (void **)&splice_len) == FAILURE ||
				!oparg || Z_TYPE_PP(splice_len) != IS_LONG) {
			THROW_EXC("Field LENGTH must be provided and must be LONG for "
					"splice at position %d", position);
			goto cleanup;
		}
		if (zend_hash_find(ht, "list", 5, (void **)&splice_val) == FAILURE ||
				!oparg || Z_TYPE_PP(splice_val) != IS_STRING) {
			THROW_EXC("Field LIST must be provided and must be STRING for "
					"splice at position %d", position);
			goto cleanup;
		}
		add_next_index_stringl(arr, Z_STRVAL_PP(opstr), 1, 1);
		add_next_index_long(arr, Z_LVAL_PP(oppos));
		add_next_index_long(arr, Z_LVAL_PP(oparg));
		add_next_index_long(arr, Z_LVAL_PP(splice_len));
		add_next_index_stringl(arr, Z_STRVAL_PP(splice_val),
				Z_STRLEN_PP(splice_val), 1);
		break;
	case '+':
	case '-':
	case '&':
	case '|':
	case '^':
	case '#':
		if (n != 3) {
			THROW_EXC("Three fields must be provided for '%s' at "
					"position %d", Z_STRVAL_PP(opstr), position);
			goto cleanup;
		}
		if (zend_hash_find(ht, "arg", 4, (void **)&oparg) == FAILURE ||
				!oparg || Z_TYPE_PP(oparg) != IS_LONG) {
			THROW_EXC("Field ARG must be provided and must be LONG for "
					"'%s' at position %d", Z_STRVAL_PP(opstr), position);
			goto cleanup;
		}
		add_next_index_stringl(arr, Z_STRVAL_PP(opstr), 1, 1);
		add_next_index_long(arr, Z_LVAL_PP(oppos));
		add_next_index_long(arr, Z_LVAL_PP(oparg));
		break;
	case '=':
	case '!':
		if (n != 3) {
			THROW_EXC("Three fields must be provided for '%s' at "
					"position %d", Z_STRVAL_PP(opstr), position);
			goto cleanup;
		}
		if (zend_hash_find(ht, "arg", 4, (void **)&oparg) == FAILURE ||
				!oparg || !PHP_MP_SERIALIZABLE_PP(oparg)) {
			THROW_EXC("Field ARG must be provided and must be SERIALIZABLE for "
					"'%s' at position %d", Z_STRVAL_PP(opstr), position);
			goto cleanup;
		}
		add_next_index_stringl(arr, Z_STRVAL_PP(opstr), 1, 1);
		add_next_index_long(arr, Z_LVAL_PP(oppos));
		SEPARATE_ZVAL_TO_MAKE_IS_REF(oparg);
		add_next_index_zval(arr, *oparg);
		break;
	default:
		THROW_EXC("Unknown operation '%s' at position %d",
				Z_STRVAL_PP(opstr), position);
		goto cleanup;
	}
	return arr;
cleanup:
	zval_ptr_dtor(&arr);
	return NULL;
}

zval *bee_update_verify_args(zval *args TSRMLS_DC) {
	if (Z_TYPE_P(args) != IS_ARRAY || php_mp_is_hash(args)) {
		THROW_EXC("Provided value for update OPS must be Array");
		return NULL;
	}
	HashTable *ht = HASH_OF(args);
	size_t n = zend_hash_num_elements(ht);

	zval **op, *arr;
	ALLOC_INIT_ZVAL(arr); array_init_size(arr, n);
	size_t key_index = 0;
	for(; key_index < n; ++key_index) {
		int status = zend_hash_index_find(ht, key_index,
				                  (void **)&op);
		if (status != SUCCESS || !op) {
			THROW_EXC("Internal Array Error");
			goto cleanup;
		}
		zval *op_arr = bee_update_verify_op(*op, key_index
				TSRMLS_CC);
		if (!op_arr)
			goto cleanup;
		if (add_next_index_zval(arr, op_arr) == FAILURE) {
			THROW_EXC("Internal Array Error");
			if (op_arr)
				zval_ptr_dtor(&op_arr);
			goto cleanup;
		}
	}
	return arr;
cleanup:
	zval_ptr_dtor(&arr);
	return NULL;
}

int get_spaceno_by_name(bee_connection *obj, zval *id, zval *name TSRMLS_DC) {
	if (Z_TYPE_P(name) == IS_LONG) return Z_LVAL_P(name);
	if (Z_TYPE_P(name) != IS_STRING) {
		THROW_EXC("Space ID must be String or Long");
		return FAILURE;
	}
	int32_t space_no = bee_schema_get_sid_by_string(obj->schema,
			Z_STRVAL_P(name), Z_STRLEN_P(name));
	if (space_no != FAILURE) return space_no;

	bee_tp_update(obj->tps);
	tp_select(obj->tps, SPACE_SPACE, INDEX_SPACE_NAME, 0, 4096);
	tp_key(obj->tps, 1);
	tp_encode_str(obj->tps, Z_STRVAL_P(name), Z_STRLEN_P(name));
	tp_reqid(obj->tps, BEE_G(sync_counter)++);

	obj->value->len = tp_used(obj->tps);
	bee_tp_flush(obj->tps);

	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		return FAILURE;

	char pack_len[5] = {0, 0, 0, 0, 0};
	if (bee_stream_read(obj, pack_len, 5) != 5) {
		THROW_EXC("Can't read query from server (failed to read length)");
		return FAILURE;
	}
	size_t body_size = php_mp_unpack_package_size(pack_len);
	smart_string_ensure(obj->value, body_size);
	if (bee_stream_read(obj, obj->value->c,
				body_size) != body_size) {
		THROW_EXC("Can't read query from server (failed to read %d "
			  "bytes from server [header + body])", body_size);
		return FAILURE;
	}

	struct beex_response resp; memset(&resp, 0, sizeof(struct beex_response));
	if (php_tp_response(&resp, obj->value->c, body_size) == -1) {
		THROW_EXC("Failed to parse query");
		return FAILURE;
	}

	if (resp.error) {
		THROW_EXC("Query error %d: %.*s", resp.code, resp.error_len, resp.error);
		return FAILURE;
	}

	if (bee_schema_add_spaces(obj->schema, resp.data, resp.data_len)) {
		THROW_EXC("Failed parsing schema (space) or memory issues");
		return FAILURE;
	}
	space_no = bee_schema_get_sid_by_string(obj->schema,
			Z_STRVAL_P(name), Z_STRLEN_P(name));
	if (space_no == FAILURE)
		THROW_EXC("No space '%s' defined", Z_STRVAL_P(name));
	return space_no;
}

int get_indexno_by_name(bee_connection *obj, zval *id,
		int space_no, zval *name TSRMLS_DC) {
	if (Z_TYPE_P(name) == IS_NULL) {
		return 0;
	} else if (Z_TYPE_P(name) == IS_LONG) {
		return Z_LVAL_P(name);
	} else if (Z_TYPE_P(name) != IS_STRING) {
		THROW_EXC("Index ID must be String or Long");
		return FAILURE;
	}
	int32_t index_no = bee_schema_get_iid_by_string(obj->schema,
			space_no, Z_STRVAL_P(name), Z_STRLEN_P(name));
	if (index_no != FAILURE) return index_no;

	bee_tp_update(obj->tps);
	tp_select(obj->tps, SPACE_INDEX, INDEX_INDEX_NAME, 0, 4096);
	tp_key(obj->tps, 2);
	tp_encode_uint(obj->tps, space_no);
	tp_encode_str(obj->tps, Z_STRVAL_P(name), Z_STRLEN_P(name));
	tp_reqid(obj->tps, BEE_G(sync_counter)++);

	obj->value->len = tp_used(obj->tps);
	bee_tp_flush(obj->tps);

	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		return FAILURE;

	char pack_len[5] = {0, 0, 0, 0, 0};
	if (bee_stream_read(obj, pack_len, 5) != 5) {
		THROW_EXC("Can't read query from server (failed to read length)");
		return FAILURE;
	}
	size_t body_size = php_mp_unpack_package_size(pack_len);
	smart_string_ensure(obj->value, body_size);
	if (bee_stream_read(obj, obj->value->c,
				body_size) != body_size) {
		THROW_EXC("Can't read query from server (failed to read %d "
			  "bytes from server [header + body])", body_size);
		return FAILURE;
	}

	struct beex_response resp; memset(&resp, 0, sizeof(struct beex_response));
	if (php_tp_response(&resp, obj->value->c, body_size) == -1) {
		THROW_EXC("Failed to parse query");
		return FAILURE;
	}

	if (resp.error) {
		THROW_EXC("Query error %d: %.*s", resp.code, resp.error_len, resp.error);
		return FAILURE;
	}

	if (bee_schema_add_indexes(obj->schema, resp.data, resp.data_len)) {
		THROW_EXC("Failed parsing schema (index) or memory issues");
		return FAILURE;
	}
	index_no = bee_schema_get_iid_by_string(obj->schema,
			space_no, Z_STRVAL_P(name), Z_STRLEN_P(name));
	if (index_no == FAILURE)
		THROW_EXC("No index '%s' defined", Z_STRVAL_P(name));
	return index_no;
}

/* ####################### METHODS ####################### */

zend_class_entry *Bee_ptr;

PHP_RINIT_FUNCTION(bee) {
	return SUCCESS;
}

static void
php_bee_init_globals(zend_bee_globals *bee_globals) {
	bee_globals->sync_counter    = 0;
	bee_globals->retry_count     = 1;
	bee_globals->retry_sleep     = 0.1;
	bee_globals->timeout         = 10.0;
	bee_globals->request_timeout = 10.0;
}

ZEND_RSRC_DTOR_FUNC(php_bee_dtor)
{
	if (rsrc->ptr) {
		bee_connection *obj = (bee_connection *)rsrc->ptr;
		bee_connection_free(obj, 1 TSRMLS_CC);
		rsrc->ptr = NULL;
		// bee_connection_free((bee_connection *)rsrc->ptr, 1);
		/* Free bee_obj here (in rsrc->ptr) */
	}
}

PHP_BEE_API
zend_class_entry *php_bee_get_ce(void)
{
	return bee_ce;
}

PHP_BEE_API
zend_class_entry *php_bee_get_exception(void)
{
	return bee_exception_ce;
}

PHP_MINIT_FUNCTION(bee) {
	/* Init global variables */
	ZEND_INIT_MODULE_GLOBALS(bee, php_bee_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	#define RLCI(NAME) REGISTER_LONG_CONSTANT("BEE_ITER_" # NAME,	\
						  ITERATOR_ ## NAME,		\
						  CONST_CS | CONST_PERSISTENT)

	/* Register constants: DEPRECATED */
	RLCI(EQ);
	RLCI(REQ);
	RLCI(ALL);
	RLCI(LT);
	RLCI(LE);
	RLCI(GE);
	RLCI(GT);
	RLCI(BITSET_ALL_SET);
	RLCI(BITSET_ANY_SET);
	RLCI(BITSET_ALL_NOT_SET);
	RLCI(OVERLAPS);
	RLCI(NEIGHBOR);

	#undef RLCI

	le_bee = zend_register_list_destructors_ex(NULL, php_bee_dtor,
			"Bee persistent connection", module_number);

	/* Init class entries */
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "Bee", Bee_methods);
	bee_ce = zend_register_internal_class(&ce TSRMLS_CC);
	bee_ce->create_object = bee_create;

	#define REGISTER_BEEX_CLASS_CONST_LONG(NAME)				\
		zend_declare_class_constant_long(php_bee_get_ce(),	\
				ZEND_STRS( #NAME ) - 1, NAME TSRMLS_CC)

	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_EQ);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_REQ);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_ALL);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_LT);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_LE);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_GE);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_GT);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_BITS_ALL_SET);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_BITSET_ALL_SET);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_BITS_ANY_SET);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_BITSET_ANY_SET);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_BITS_ALL_NOT_SET);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_BITSET_ALL_NOT_SET);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_OVERLAPS);
	REGISTER_BEEX_CLASS_CONST_LONG(ITERATOR_NEIGHBOR);

	#undef REGISTER_BEEX_CLASS_CONST_LONG

	INIT_CLASS_ENTRY(ce, "BeeException", NULL);
	bee_exception_ce = zend_register_internal_class_ex(&ce,
			php_bee_get_exception_base(0 TSRMLS_CC),
			NULL TSRMLS_CC);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(bee) {
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_MINFO_FUNCTION(bee) {
	php_info_print_table_start();
	php_info_print_table_header(2, "Bee support", "enabled");
	php_info_print_table_row(2, "Extension version", PHP_BEE_VERSION);
	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
}

static int php_bee_list_entry() {
	return le_bee;
}

PHP_METHOD(Bee, __construct) {
	char *host = NULL, *login = NULL, *passwd = NULL;
	int host_len = 0, login_len = 0, passwd_len = 0;
	long port = 0;
	int is_persistent = 0, plist_new_entry = 1;

	const char *plist_id = NULL, *suffix = NULL;
	int plist_id_len = 0, suffix_len = 0;

	BEE_PARSE_PARAMS(id, "|slss!s", &host, &host_len, &port,
			       &login, &login_len, &passwd, &passwd_len,
			       &suffix, &suffix_len);
	BEE_FETCH_OBJECT(obj, id);

	if (host == NULL) {
		host = "localhost";
	}

	if (port < 0 || port >= 65536) {
		THROW_EXC("Invalid primary port value: %li", port);
		RETURN_FALSE;
	} else if (port == 0) {
		port = 3301;
	}
	if (login == NULL) {
		login = "guest";
	}
	if (passwd != NULL && passwd_len == 0) {
		passwd = NULL;
	}

	/* Not sure how persistency and ZTS are combined*/
	/* #ifndef   ZTS */
	/* Do not allow not persistent connections, for now */
	is_persistent = (BEE_G(persistent) || suffix ? 1 : 0);
	/* #endif *//* ZTS */

	if (is_persistent) {
		zend_rsrc_list_entry *le = NULL;

		plist_id = persistent_id(host, port, login, "plist",
					 &plist_id_len, suffix, suffix_len);

		if (BEE_PERSISTENT_FIND(plist_id, plist_id_len, le) ==
		    SUCCESS) {
			/* It's unlikely */
			if (le->type == php_bee_list_entry()) {
				obj = (struct bee_connection *) le->ptr;
				plist_new_entry = 0;
			}
		}
		t_obj->obj = obj;
	}

	if (obj == NULL) {
		obj = pecalloc(1, sizeof(bee_connection),
				 is_persistent);
		if (obj == NULL) {
			if (plist_id) {
				pefree((void *)plist_id, 1);
				plist_id = NULL;
			}
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "out of "
					 "memory: cannot allocate handle");
		}

		/* initialzie object structure */
		obj->host = pestrdup(host, is_persistent);
		obj->port = port;
		obj->value = (smart_string *)pecalloc(1,sizeof(smart_string),1);
		/* CHECK obj->value */
		memset(obj->value, 0, sizeof(smart_string));
		smart_string_ensure(obj->value, GREETING_SIZE);
		obj->greeting = (char *)pecalloc(GREETING_SIZE, sizeof(char),
						 is_persistent);
		/* CHECK obj->greeting */
		obj->salt = obj->greeting + SALT_PREFIX_SIZE;
		obj->login = pestrdup(login, is_persistent);
		obj->orig_login = pestrdup(login, is_persistent);
		/* If passwd == NULL, then authenticate without password */
		if (passwd) {
			obj->passwd = pestrdup(passwd, is_persistent);
		}
		if (is_persistent) {
			obj->persistent_id = persistent_id(host, port, login,
							   "stream", NULL,
							   suffix, suffix_len);
		}
		obj->schema = bee_schema_new(is_persistent);
		/* CHECK obj->schema */
		obj->tps = bee_tp_new(obj->value, is_persistent);
		/* CHECK obj->tps */
	}

	if (is_persistent && plist_new_entry) {
		zend_rsrc_list_entry le;
		memset(&le, 0, sizeof(zend_rsrc_list_entry));

		le.type = php_bee_list_entry();
		le.ptr  = obj;
		if (zend_hash_update(&EG(persistent_list), plist_id,
				     plist_id_len - 1, (void *)&le,
				     sizeof(zend_rsrc_list_entry),
				     NULL) == FAILURE) {
			if (plist_id) {
				pefree((void *)plist_id, 1);
				plist_id = NULL;
			}
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "could not "
					 "register persistent entry");
		}
	}
	t_obj->obj = obj;
	t_obj->is_persistent = is_persistent;

	if (plist_id) {
		pefree((void *)plist_id, 1);
	}
	return;
}

PHP_METHOD(Bee, connect) {
	BEE_PARSE_PARAMS(id, "", id);
	BEE_FETCH_OBJECT(obj, id);
	if (obj->stream && obj->stream->mode) RETURN_TRUE;
	if (__bee_connect(t_obj, id TSRMLS_CC) == FAILURE)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_METHOD(Bee, reconnect) {
	BEE_PARSE_PARAMS(id, "", id);
	BEE_FETCH_OBJECT(obj, id);
	if (__bee_reconnect(t_obj, id TSRMLS_CC) == FAILURE)
		RETURN_FALSE;
	RETURN_TRUE;
}

int __bee_authenticate(bee_connection *obj) {
	TSRMLS_FETCH();

	bee_schema_flush(obj->schema);
	bee_tp_update(obj->tps);
	int batch_count = 3;
	size_t passwd_len = (obj->passwd ? strlen(obj->passwd) : 0);
	tp_auth(obj->tps, obj->salt, obj->login, strlen(obj->login),
		obj->passwd, passwd_len);
	uint32_t auth_sync = BEE_G(sync_counter)++;
	tp_reqid(obj->tps, auth_sync);
	tp_select(obj->tps, SPACE_SPACE, 0, 0, 4096);
	tp_key(obj->tps, 0);
	uint32_t space_sync = BEE_G(sync_counter)++;
	tp_reqid(obj->tps, space_sync);
	tp_select(obj->tps, SPACE_INDEX, 0, 0, 4096);
	tp_key(obj->tps, 0);
	uint32_t index_sync = BEE_G(sync_counter)++;
	tp_reqid(obj->tps, index_sync);
	obj->value->len = tp_used(obj->tps);
	bee_tp_flush(obj->tps);

	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		return FAILURE;

	int status = SUCCESS;

	while (batch_count-- > 0) {
		char pack_len[5] = {0, 0, 0, 0, 0};
		if (bee_stream_read(obj, pack_len, 5) != 5) {
			THROW_EXC("Can't read query from server");
			return FAILURE;
		}
		size_t body_size = php_mp_unpack_package_size(pack_len);
		smart_string_ensure(obj->value, body_size);
		if (bee_stream_read(obj, obj->value->c,
					body_size) != body_size) {
			THROW_EXC("Can't read query from server");
			return FAILURE;
		}
		if (status == FAILURE) continue;
		struct beex_response resp;
		memset(&resp, 0, sizeof(struct beex_response));
		if (php_tp_response(&resp, obj->value->c, body_size) == -1) {
			THROW_EXC("Failed to parse query");
			status = FAILURE;
		}

		if (resp.error) {
			THROW_EXC("Query error %d: %.*s", resp.code,
				  resp.error_len, resp.error);
			status = FAILURE;
		}
		if (resp.sync == space_sync) {
			if (bee_schema_add_spaces(obj->schema, resp.data,
						        resp.data_len) &&
					status != FAILURE) {
				THROW_EXC("Failed parsing schema (space) or "
					  "memory issues");
				status = FAILURE;
			}
		} else if (resp.sync == index_sync) {
			if (bee_schema_add_indexes(obj->schema, resp.data,
							 resp.data_len) &&
					status != FAILURE) {
				THROW_EXC("Failed parsing schema (index) or "
					  "memory issues");
				status = FAILURE;
			}
		} else if (resp.sync == auth_sync && resp.error) {
			THROW_EXC("Query error %d: %.*s", resp.code,
				  resp.error_len, resp.error);
			status = FAILURE;
		}
	}

	return status;
}

PHP_METHOD(Bee, authenticate) {
	char *login; int login_len;
	char *passwd = NULL; int passwd_len = 0;

	BEE_PARSE_PARAMS(id, "s|s!", &login, &login_len,
			&passwd, &passwd_len);
	BEE_FETCH_OBJECT(obj, id);
	if (obj->login != NULL) {
		pefree(obj->login, t_obj->is_persistent);
		obj->login = NULL;
	}
	obj->login = pestrdup(login, t_obj->is_persistent);
	if (obj->passwd != NULL) {
		pefree(obj->passwd, t_obj->is_persistent);
		obj->passwd = NULL;
	}
	if (passwd != NULL) {
		obj->passwd = pestrdup(passwd, t_obj->is_persistent);
	}
	BEE_CONNECT_ON_DEMAND(obj, id);

	__bee_authenticate(obj);
	RETURN_NULL();
}

PHP_METHOD(Bee, flush_schema) {
	BEE_PARSE_PARAMS(id, "", id);
	BEE_FETCH_OBJECT(obj, id);

	bee_schema_flush(obj->schema);
	RETURN_TRUE;
}

PHP_METHOD(Bee, close) {
	BEE_PARSE_PARAMS(id, "", id);
	BEE_FETCH_OBJECT(obj, id);

	RETURN_TRUE;
}

PHP_METHOD(Bee, ping) {
	BEE_PARSE_PARAMS(id, "", id);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	long sync = BEE_G(sync_counter)++;
	php_tp_encode_ping(obj->value, sync);
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval_ptr_dtor(&header);
	zval_ptr_dtor(&body);
	RETURN_TRUE;
}

PHP_METHOD(Bee, select) {
	zval *space = NULL, *index = NULL;
	zval *key = NULL, *key_new = NULL;
	zval *zlimit = NULL, *iterator = NULL;
	long limit = LONG_MAX-1, offset = 0;

	BEE_PARSE_PARAMS(id, "z|zzzlz", &space, &key,
			&index, &zlimit, &offset, &iterator);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	if (zlimit != NULL && Z_TYPE_P(zlimit) != IS_NULL && Z_TYPE_P(zlimit) != IS_LONG) {
		THROW_EXC("wrong type of 'limit' - expected long/null, got '%s'",
				zend_zval_type_name(zlimit));
		RETURN_FALSE;
	} else if (zlimit != NULL && Z_TYPE_P(zlimit) == IS_LONG) {
		limit = Z_LVAL_P(zlimit);
	}

	long space_no = get_spaceno_by_name(obj, id, space TSRMLS_CC);
	if (space_no == FAILURE) RETURN_FALSE;
	int32_t index_no = 0;
	if (index) {
		index_no = get_indexno_by_name(obj, id, space_no, index TSRMLS_CC);
		if (index_no == FAILURE) RETURN_FALSE;
	}

	int is_iterator_all = (!key ||
		Z_TYPE_P(key) == IS_NULL ||
		(Z_TYPE_P(key) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(key)) == 0)
	);
	int iterator_id = convert_iterator(iterator, is_iterator_all);
	if (iterator_id == -1)
		RETURN_FALSE;

	key_new = pack_key(key, 1);

	long sync = BEE_G(sync_counter)++;
	php_tp_encode_select(obj->value, sync, space_no, index_no,
			     limit, offset, iterator_id, key_new);
	if (key != key_new)
		zval_ptr_dtor(&key_new);
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header = NULL, *body = NULL;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}

PHP_METHOD(Bee, insert) {
	zval *space, *tuple;

	BEE_PARSE_PARAMS(id, "za", &space, &tuple);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	long space_no = get_spaceno_by_name(obj, id, space TSRMLS_CC);
	if (space_no == FAILURE)
		RETURN_FALSE;

	long sync = BEE_G(sync_counter)++;
	php_tp_encode_insert_or_replace(obj->value, sync, space_no,
			tuple, BEEX_INSERT);
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}

PHP_METHOD(Bee, replace) {
	zval *space, *tuple;

	BEE_PARSE_PARAMS(id, "za", &space, &tuple);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	long space_no = get_spaceno_by_name(obj, id, space TSRMLS_CC);
	if (space_no == FAILURE)
		RETURN_FALSE;

	long sync = BEE_G(sync_counter)++;
	php_tp_encode_insert_or_replace(obj->value, sync, space_no,
			tuple, BEEX_REPLACE);
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}

PHP_METHOD(Bee, delete) {
	zval *space = NULL, *key = NULL, *index = NULL;
	zval *key_new = NULL;

	BEE_PARSE_PARAMS(id, "zz|z", &space, &key, &index);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	long space_no = get_spaceno_by_name(obj, id, space TSRMLS_CC);
	if (space_no == FAILURE) RETURN_FALSE;
	int32_t index_no = 0;
	if (index) {
		index_no = get_indexno_by_name(obj, id, space_no, index TSRMLS_CC);
		if (index_no == FAILURE) RETURN_FALSE;
	}

	key_new = pack_key(key, 0);

	long sync = BEE_G(sync_counter)++;
	php_tp_encode_delete(obj->value, sync, space_no, index_no, key_new);
	if (key != key_new) {
		zval_ptr_dtor(&key_new);
	}
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}

PHP_METHOD(Bee, call) {
	char *proc; size_t proc_len;
	zval *tuple = NULL;

	BEE_PARSE_PARAMS(id, "s|z", &proc, &proc_len, &tuple);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	zval *tuple_new = pack_key(tuple, 1);

	long sync = BEE_G(sync_counter)++;
	php_tp_encode_call(obj->value, sync, proc, proc_len, tuple_new);
	if (tuple_new != tuple) {
		zval_ptr_dtor(&tuple_new);
	}
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}

PHP_METHOD(Bee, eval) {
	char *proc; size_t proc_len;
	zval *tuple = NULL;

	BEE_PARSE_PARAMS(id, "s|z", &proc, &proc_len, &tuple);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	zval *tuple_new = pack_key(tuple, 1);

	long sync = BEE_G(sync_counter)++;
	php_tp_encode_eval(obj->value, sync, proc, proc_len, tuple_new);
	if (tuple_new != tuple) {
		zval_ptr_dtor(&tuple_new);
	}
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}

PHP_METHOD(Bee, update) {
	zval *space = NULL, *key = NULL, *index = NULL, *args = NULL;
	zval *key_new = NULL;

	BEE_PARSE_PARAMS(id, "zza|z", &space, &key, &args, &index);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	long space_no = get_spaceno_by_name(obj, id, space TSRMLS_CC);
	if (space_no == FAILURE) RETURN_FALSE;
	int32_t index_no = 0;
	if (index) {
		index_no = get_indexno_by_name(obj, id, space_no, index TSRMLS_CC);
		if (index_no == FAILURE) RETURN_FALSE;
	}

	args = bee_update_verify_args(args TSRMLS_CC);
	if (!args) RETURN_FALSE;
	key_new = pack_key(key, 0);
	long sync = BEE_G(sync_counter)++;
	php_tp_encode_update(obj->value, sync, space_no, index_no, key_new, args);
	if (key != key_new) {
		zval_ptr_dtor(&key_new);
	}
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}

PHP_METHOD(Bee, upsert) {
	zval *space = NULL, *tuple = NULL, *args = NULL;

	BEE_PARSE_PARAMS(id, "zaa", &space, &tuple, &args);
	BEE_FETCH_OBJECT(obj, id);
	BEE_CONNECT_ON_DEMAND(obj, id);

	long space_no = get_spaceno_by_name(obj, id, space TSRMLS_CC);
	if (space_no == FAILURE) RETURN_FALSE;

	args = bee_update_verify_args(args TSRMLS_CC);
	if (!args) RETURN_FALSE;
	long sync = BEE_G(sync_counter)++;
	php_tp_encode_upsert(obj->value, sync, space_no, tuple, args);
	if (bee_stream_send(obj TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	zval *header, *body;
	if (bee_step_recv(obj, sync, &header, &body TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	BEE_RETURN_DATA(body, header, body);
}
