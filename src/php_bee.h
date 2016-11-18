#ifndef    PHP_BEE_H
#define    PHP_BEE_H

#include <php.h>
#include <php_ini.h>
#include <zend_API.h>
#include <php_network.h>
#include <zend_compile.h>
#include <zend_exceptions.h>

#include <ext/standard/info.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if PHP_VERSION_ID >= 70000
#  include <ext/standard/php_smart_string.h>
#else
#  include <ext/standard/php_smart_str.h>
   typedef smart_str smart_string;
#  define smart_string_alloc4(...) smart_str_alloc4(__VA_ARGS__)
#  define smart_string_free_ex(...) smart_str_free_ex(__VA_ARGS__)
#endif

extern zend_module_entry bee_module_entry;
#define phpext_bee_ptr &bee_module_entry

#define PHP_BEE_VERSION "0.1.0"
#define PHP_BEE_EXTNAME "bee"

#ifdef PHP_WIN32
#  define PHP_BEE_API __declspec(__dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define PHP_BEE_API __attribute__ ((visibility("default")))
#else
#  define PHP_BEE_API
#endif

#define BEE_TIMEOUT_SEC 10
#define BEE_TIMEOUT_USEC 0

#ifdef ZTS
#include "TSRM.h"
#endif

struct bee_schema;

#define SSTR_BEG(str) (str->c)
#define SSTR_END(str) (str->c + str->a)
#define SSTR_AWA(str) (str->a)
#define SSTR_LEN(str) (str->len)
#define SSTR_POS(str) (str->c + str->len)
#define SSTR_DIF(str, end) (end - str->c)

PHP_MINIT_FUNCTION(bee);
PHP_RINIT_FUNCTION(bee);
PHP_MSHUTDOWN_FUNCTION(bee);
PHP_MINFO_FUNCTION(bee);

PHP_METHOD(Bee, __construct);
PHP_METHOD(Bee, connect);
PHP_METHOD(Bee, reconnect);
PHP_METHOD(Bee, close);
PHP_METHOD(Bee, authenticate);
PHP_METHOD(Bee, ping);
PHP_METHOD(Bee, select);
PHP_METHOD(Bee, insert);
PHP_METHOD(Bee, replace);
PHP_METHOD(Bee, call);
PHP_METHOD(Bee, eval);
PHP_METHOD(Bee, delete);
PHP_METHOD(Bee, update);
PHP_METHOD(Bee, upsert);
PHP_METHOD(Bee, flush_schema);

ZEND_BEGIN_MODULE_GLOBALS(bee)
	zend_bool persistent;
	long sync_counter;
	long retry_count;
	double retry_sleep;
	double timeout;
	double request_timeout;
ZEND_END_MODULE_GLOBALS(bee)

ZEND_EXTERN_MODULE_GLOBALS(bee);

typedef struct bee_object {
	zend_object   zo;

	struct bee_connection  {
		char             *host;
		int               port;
		char             *login;
		char             *passwd;
		php_stream       *stream;
		struct bee_schema *schema;
		smart_string     *value;
		struct tp        *tps;
		char             *greeting;
		char             *salt;
		/* Only for persistent connections */
		char             *orig_login;
		char             *suffix;
		int               suffix_len;
		char             *persistent_id;
	}            *obj;

	zend_bool     is_persistent;
} bee_object;

typedef struct bee_connection bee_connection;

PHP_BEE_API zend_class_entry *php_bee_get_ce(void);
PHP_BEE_API zend_class_entry *php_bee_get_exception(void);
PHP_BEE_API zend_class_entry *php_bee_get_exception_base(int root TSRMLS_DC);

#ifdef ZTS
#  define BEE_G(v) TSRMG(bee_globals_id, zend_bee_globals *, v)
#else
#  define BEE_G(v) (bee_globals.v)
#endif

#define THROW_EXC(...) zend_throw_exception_ex(					\
	zend_exception_get_default(TSRMLS_C), 0 TSRMLS_CC, __VA_ARGS__)

#endif  /* PHP_BEE_H */
