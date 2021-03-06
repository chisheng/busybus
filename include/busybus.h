/*
 * Copyright (C) 2013 Bartosz Golaszewski <bartekgola@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * @mainpage Busybus public API
 *
 * This is the busybus public API documentation.
 *
 * <p>These functions and macros are all that is needed in order to register
 * and call busybus methods, as well as to create bindings in other languages
 * and even to build fully functional clients and servers. In fact bbusd has
 * been built utilising this public API exclusively.
 */

#ifndef __BUSYBUS__
#define __BUSYBUS__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

/**
 * @defgroup __common__ Common functions and macros
 * @{
 *
 * Commonly used functions handling memory allocation, string
 * manipulation etc, and some utility macros.
 */

/**
 * @brief Expands to '1'.
 */
#define BBUS_TRUE 1

/**
 * @brief Expands to '0'.
 */
#define BBUS_FALSE 0

/**
 * @brief Makes symbol visible.
 */
#define BBUS_PUBLIC __attribute__((visibility("default")))

/**
 * @brief Marks function as non-returning.
 */
#define BBUS_NORETURN __attribute__((noreturn))

/**
 * @brief Marks function as being "printf-like".
 * @param FORMAT Position of the format parameter in the argument list.
 * @param PARAMS Position of the first of the variadic arguments.
 *
 * Makes preprocessor verify that the variadic arguments' types match
 * arguments specified in the format string.
 */
#define BBUS_PRINTF_FUNC(FORMAT, PARAMS)				\
		__attribute__((format(printf, FORMAT, PARAMS)))

/**
 * @brief Marks a function's argument as unused.
 */
#define BBUS_UNUSED __attribute__((unused))

/**
 * @brief Used to get the number of elements in an array.
 */
#define BBUS_ARRAY_SIZE(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

/**
 * @brief Provide a branch prediction hint - expect the statement to be true.
 * @param EXPR Statement expected to evaluate to true.
 */
#define BBUS_LIKELY(EXPR) __builtin_expect((EXPR), 1)

/**
 * @brief Provide a branch prediction hint - expect the statement to be false.
 * @param EXPR Statement expected to evaluate to false.
 */
#define BBUS_UNLIKELY(EXPR) __builtin_expect((EXPR), 0)

/**
 * @brief Functions with this attribute will be run before entering main().
 */
#define BBUS_ATSTART __attribute__((constructor))

/**
 * @brief Marks a function as a constructor with high priority.
 *
 * Functions marked with this macro will be run before the main() function
 * and before any other function marked with one of the ATSTART macros.
 */
#define BBUS_ATSTART_FIRST __attribute__((constructor(1000)))

/**
 * @brief Marks a function as a constructor with low priority.
 *
 * Functions marked with this macro will be run before the main() function
 * and after any other function marked with one of the ATSTART macros.
 */
#define BBUS_ATSTART_LAST __attribute__((constructor(2000)))

/**
 * @brief Variables with this specifier have a separate instance per thread.
 */
#define BBUS_THREAD_LOCAL __thread

/**
 * @brief Busybus malloc.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory or NULL in case of an error.
 *
 * Returns a valid pointer even for equal to zero.
 */
void* bbus_malloc(size_t size) BBUS_PUBLIC;

/**
 * @brief Works just like bbus_malloc, but zeroes allocated memory.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory or NULL in case of an error.
 */
void* bbus_malloc0(size_t size) BBUS_PUBLIC;

/**
 * @brief Busybus realloc.
 * @param ptr Memory, that needs reallocation
 * @param size Number of bytes to allocate.
 *
 * Returns a valid pointer for size equal to zero, for NULL ptr behaves
 * like bbus_malloc.
 */
void* bbus_realloc(void* ptr, size_t size) BBUS_PUBLIC;

/**
 * @brief Busybus free.
 * @param ptr Memory to be freed.
 *
 * Does nothing if ptr is NULL. It's not safe to use bbus_free on memory
 * allocated by regular malloc and vice-versa.
 */
void bbus_free(void* ptr) BBUS_PUBLIC;

/**
 * @brief Duplicates memory area.
 * @param src Memory to duplicate.
 * @param size Number of bytes to duplicate.
 * @return Pointer to the newly allocated buffer.
 *
 * Allocates new memory using bbus_malloc() - must be freed using bbus_free().
 */
void* bbus_memdup(const void* src, size_t size) BBUS_PUBLIC;

/**
 * @brief Atomically accesses the value of a variable and returns it.
 * @param VAR The variable to access.
 */
#define BBUS_ATOMIC_GET(VAR) __sync_fetch_and_or(&(VAR), 0)

/**
 * @brief Atomically sets new value for a variable.
 * @param VAR The variable to access.
 * @param VAL New value.
 */
#define BBUS_ATOMIC_SET(VAR, VAL)					\
	do {								\
		(void)__sync_lock_test_and_set(&(VAR), (VAL));		\
	} while (0)

/**
 * @brief Atomically writes VAL to VAR and returns previous contents of VAR.
 * @param VAR The variable to set.
 * @param VAL New value to write.
 */
#define BBUS_ATOMIC_LOCK_TEST_AND_SET(VAR, VAL)				\
	__sync_lock_test_and_set(&(VAR), (VAL))

/**
 * @brief Atomically releases the lock acquired by LOCK_TEST_AND_SET.
 * @param VAR The variable to release.
 *
 * This writes the constant 0 to VAR.
 */
#define BBUS_ATOMIC_LOCK_RELEASE(VAR) (void)__sync_lock_release(&(VAR))

/**
 * @brief Represents an elapsed time.
 */
struct bbus_timeval
{
	long int sec;	/**< Number of seconds. */
	long int usec;	/**< Number of miliseconds. */
};

/**
 * @brief Build a string from given format and arguments.
 * @param fmt Format of the string to be built.
 * @return Pointer to the newly allocated string or NULL if an error occurred.
 *
 * Returned string must be freed using bbus_str_free.
 */
char* bbus_str_build(const char* fmt, ...) BBUS_PUBLIC BBUS_PRINTF_FUNC(1, 2);

/**
 * @brief Copy a string.
 * @param str Source string to be copied.
 * @return Pointer to the newly allocated string or NULL if an error occurred.
 *
 * Works more like strdup, than strcpy as it allocates it's own buffer using
 * bbus_malloc(). Returned string must be freed using bbus_str_free.
 */
char* bbus_str_cpy(const char* str) BBUS_PUBLIC;

/**
 * @brief Joins two strings, of which the first one must be a bbus string.
 * @param dst String allocated by one of the bbus string functions.
 * @param src Source string.
 * @return Pointer to joined strings or NULL on no memory.
 *
 * This functions tries to realloc 'dst'. It means it may be moved to another
 * location. If this functions fails, 'dst' is left untouched - it is neither
 * freed nor moved.
 */
char* bbus_str_join(char* dst, const char* src) BBUS_PUBLIC;

/**
 * @brief Frees a string allocated by one of the bbus string functions.
 * @param str Pointer to the string that will be freed.
 */
void bbus_str_free(char* str) BBUS_PUBLIC;

/**
 * @brief Matches a string against given regular expression pattern.
 * @param pattern A valid POSIX regular expression pattern.
 * @param str String to be matched.
 * @return BBUS_TRUE on match, BBUS_FALSE on no-match and -1 on error.
 */
int bbus_regex_match(const char* pattern, const char* str) BBUS_PUBLIC;

/**
 * @brief Computes crc32 checksum of given data.
 * @param buf Buffer containing the data.
 * @param bufsize Size of the data to be computed.
 * @return Crc32 checksum.
 */
uint32_t bbus_crc32(const void* buf, size_t bufsize) BBUS_PUBLIC;

/**
 * @brief For given uid returns the name of the user.
 * @param uid The user ID.
 * @param buf Buffer to store the username.
 * @param buflen Size of the buffer.
 * @return 0 on success, -1 on error.
 */
int bbus_cred_uidtousername(uid_t uid, char* buf, size_t buflen) BBUS_PUBLIC;

/**
 * @brief Returns the name of the process with given pid.
 * @param pid Process ID.
 * @param buf Buffer to store the process name.
 * @param buflen Size of the buffer.
 * @return 0 if the name has been properly read, -1 on error.
 *
 * This function will fail if the proc filesystem is not mounted, or the
 * calling process doesn't have the permissions to read it.
 */
int bbus_proc_pidtoname(pid_t pid, char* buf, size_t buflen) BBUS_PUBLIC;

/**
 * @brief Returns the smallest of two values.
 * @param A First value.
 * @param B Second value.
 */
#define BBUS_MIN(A, B)							\
	({								\
		__typeof__(A) _A = (A);					\
		__typeof__(B) _B = (B);					\
		_A < _B ? _A : _B;					\
	})

/**
 * @brief Represents a single element in the doubly-linked list.
 *
 * Structures passed as list element arguments to the list manipulation
 * functions must too have their first two fields be pointers to the next
 * and previous list elements.
 */
struct bbus_list_elem
{
	struct bbus_list_elem* next;	/**< Next element. */
	struct bbus_list_elem* prev;	/**< Previous element. */
	char data[1];			/**< Arbitrary data. */
};

/**
 * @brief Represents a doubly-linked list.
 *
 * Structures passed as the 'list' argument to the list manipulation functions
 * must too be composed of two fields that are pointers to list elements.
 */
struct bbus_list
{
	struct bbus_list_elem* head;	/**< First element in the list. */
	struct bbus_list_elem* tail;	/**< Last element in the list. */
};

/**
 * @brief Inserts an element at the end of a doubly-linked list.
 * @param list The list.
 * @param elem New element.
 */
void bbus_list_push(void* list, void* elem) BBUS_PUBLIC;

/**
 * @brief Inserts an element into a doubly-linked list.
 * @param list The list.
 * @param elem New element.
 * @param prev The element that should precede 'elem'.
 */
void bbus_list_insert(void* list, void* elem, void* prev) BBUS_PUBLIC;

/**
 * @brief Removes an element from a doubly linked list.
 * @param list The list.
 * @param elem The element to remove.
 */
void bbus_list_rm(void* list, void* elem) BBUS_PUBLIC;

/**
 * @}
 *
 * @defgroup __hashmap__ Hashmap functions
 * @{
 *
 * Busybus hashmap interface. Both keys and values can be pointers to any data
 * types, hashes are computed directly from memory buffers.
 */

/**
 * @brief Opaque hashmap object. Only accessible through interface functions.
 */
typedef struct __bbus_hashmap bbus_hashmap;

/**
 * @brief Indicates the type of a key in the hashmap.
 */
enum bbus_hmap_type
{
	BBUS_HMAP_KEYUINT = 1,	/**< Keys are unsigned integers. */
	BBUS_HMAP_KEYSTR,	/**< Keys are null-terminated strings. */
};

/**
 * @brief Creates an empty hashmap object.
 * @return Pointer to the new hashmap or NULL if no memory.
 */
bbus_hashmap* bbus_hmap_create(enum bbus_hmap_type type) BBUS_PUBLIC;

/**
 * @brief Inserts an entry or sets a new value for an existing one.
 * @param hmap The hashmap.
 * @param key The key.
 * @param val New value.
 * @return 0 on success, -1 if no memory.
 *
 * A string is used as key.
 */
int bbus_hmap_setstr(bbus_hashmap* hmap,
		const char* key, void* val) BBUS_PUBLIC;

/**
 * @brief Looks up the value for a given string.
 * @param hmap The hashmap.
 * @param key The key.
 * @return Pointer to the looked up entry or NULL if not present.
 */
void* bbus_hmap_findstr(bbus_hashmap* hmap, const char* key) BBUS_PUBLIC;

/**
 * @brief Removes an entry for a given string.
 * @param hmap The hashmap.
 * @param key The key.
 * @return Pointer to the looked up value or NULL if not present.
 *
 * If value points to dynamically allocated data, it must be freed
 * separately.
 */
void* bbus_hmap_rmstr(bbus_hashmap* hmap, const char* key) BBUS_PUBLIC;

/**
 * @brief Inserts an entry or sets a new value for an existing one.
 * @param hmap The hashmap.
 * @param key The key.
 * @param val New value.
 * @return 0 on success, -1 if no memory.
 *
 * An unsigned integer is used as key.
 */
int bbus_hmap_setuint(bbus_hashmap* hmap, unsigned key, void* val) BBUS_PUBLIC;

/**
 * @brief Looks up the value for a given unsigned integer.
 * @param hmap The hashmap.
 * @param key The key.
 * @return Pointer to the looked up entry or NULL if not present.
 */
void* bbus_hmap_finduint(bbus_hashmap* hmap, unsigned key) BBUS_PUBLIC;

/**
 * @brief Removes an entry for a given unsigned integer.
 * @param hmap The hashmap.
 * @param key The key.
 * @return Pointer to the looked up value or NULL if not present.
 *
 * If value points to dynamically allocated data, it must be freed
 * separately.
 */
void* bbus_hmap_rmuint(bbus_hashmap* hmap, unsigned key) BBUS_PUBLIC;

/**
 * @brief Deletes all key-value pairs from the hashmap.
 * @param hmap Hashmap to reset.
 *
 * Hmap remains a valid hashmap object and must be freed.
 */
void bbus_hmap_reset(bbus_hashmap* hmap) BBUS_PUBLIC;

/**
 * @brief Frees a hashmap object.
 * @param hmap Hashmap to free.
 *
 * Does nothing to the value pointers, so it's possible to use
 * statically allocated data structures as hashmap values.
 *
 * If 'hmap' is a NULL-pointer, this function does nothing.
 */
void bbus_hmap_free(bbus_hashmap* hmap) BBUS_PUBLIC;

/**
 * @brief Converts the hashmap's contents into human-readable form.
 * @param hmap Hashmap object to dump.
 * @param buf Buffer to store the data.
 * @param bufsize Size of the buffer.
 * @return 0 if the hashmap has been properly dumped, -1 in case of an error.
 *
 * Buffer must be big enough to store the output, or this function will exit
 * with an error, although even then it's possible to read the content
 * that has been already written to buf.
 */
int bbus_hmap_dump(bbus_hashmap* hmap, char* buf, size_t bufsize) BBUS_PUBLIC;

/**
 * @}
 *
 * @defgroup __args__ Argument parsing
 * @{
 *
 * Functions and data structures for easy argument parsing.
 */

/**
 * @brief Indicates the action which should be performed for an option.
 */
enum bbus_opt_action
{
	BBUS_OPTACT_NOTHING = 0,	/**< Do nothing. */
	BBUS_OPTACT_SETFLAG,		/**< Set an integer variable to 1. */
	BBUS_OPTACT_GETOPTARG,		/**< Store the optarg argument. */
	BBUS_OPTACT_CALLFUNC,		/**< Call a registered callback. */
};

/**
 * @brief Indicates the argument requirements of an option.
 */
enum bbus_opt_hasarg
{
	BBUS_OPT_NOARG = 0,	/**< Option doesn't accept an argument. */
	BBUS_OPT_ARGREQ,	/**< Option requires an argument. */
	BBUS_OPT_ARGOPT,	/**< Argument is optional for this option. */
};

/**
 * @brief Signature of callbacks called for options with CALLFUNC action set.
 */
typedef void (*bbus_opt_callback)(const char* arg);

/**
 * @brief Describes a single option recognized by bbus_parse_args().
 */
struct bbus_option
{
	int shortopt;			/**< Short option character. */
	const char* longopt;		/**< Long option string. */
	enum bbus_opt_hasarg hasarg;	/**< Argument requirement. */
	enum bbus_opt_action action;	/**< Action to be performed. */
	void* actdata;			/**< Action-specific data. */
	const char* descr;		/**< Description string. */
};

/**
 * @brief Describes a single positional parameter for bbus_parse_args().
 */
struct bbus_posarg
{
	enum bbus_opt_action action;	/**< Action to be performed. */
	void* actdata;			/**< Action-specific data. */
	const char* descr;		/**< Description string. */
};

/**
 * @brief Contains all needed input information for bbus_parse_args().
 */
struct bbus_opt_list
{
	const struct bbus_option* opts;	/**< List of available options. */
	size_t numopts;			/**< Number of supplied options. */
	const struct bbus_posarg* pargs;
	/**< List of expected positional parameters. */
	size_t numpargs;		/**< Number of positional arguments. */
	const char* progname;		/**< Full program name. */
	const char* version;		/**< Program version string. */
	const char* progdescr;		/**< Elaborate program description. */
};

/**
 * @brief List of remaining non-options extracted by bbus_parse_args().
 *
 * It's filled with pointers to the argv vector by bbus_parse_args(). It
 * should be freed using bbus_free_nonopts().
 */
struct bbus_nonopts
{
	char** args;		/**< List of pointers to non-options. */
	size_t numargs;		/**< Number of non-options. */
};

#define BBUS_ARGS_GOOD	0	/**< bbus_parse_args parsed all arguments. */
#define BBUS_ARGS_ERR	-1	/**< bbus_parse_args encountered an error */
#define BBUS_ARGS_HELP	1	/**< bbus_parse_args showed help or version. */

/**
 * @brief Parses the command-line arguments according to given list of options.
 * @param argc Number of arguments.
 * @param argv Array of comand-line arguments.
 * @param optlist Program description and list of available options.
 * @param nonopts If not NULL, the remaining non-options will be stored here.
 * @return Returns 0 on success, -1 on failure.
 *
 * Internally this function uses getopt_long() to parse the command-line
 * arguments, but extends it in that it automatically generates the help and
 * version messages and offers a basic callback interface.
 *
 * This function is different from other busybus public functions in that
 * instead of setting the global error number on failure it prints the error
 * message to stderr. Global error code can still be set by internally used
 * busybus functions but should be ignored.
 */
int bbus_parse_args(int argc, char** argv, const struct bbus_opt_list* optlist,
		struct bbus_nonopts** nonopts) BBUS_PUBLIC;

/**
 * @brief Frees non-options collected by bbus_parse_args().
 * @param nonopts The non-options struct.
 */
void bbus_free_nonopts(struct bbus_nonopts* nonopts) BBUS_PUBLIC;

/**
 * @}
 *
 * @defgroup __environ__ Environment variables
 * @{
 *
 * Environment variables that can be set to affect the behavior of the
 * busybus library.
 */

/**
 * @brief Path to the busybus unix socket.
 *
 * Affects the default socket path. If this variable is set libbbus will use
 * its value instead of the default path specified in busybus.h. In most
 * busybus programs however this can be overridden by socket path passed as a
 * command-line argument e.g. '--sockpath' in bbusd.
 */
#define BBUS_ENV_SOCKPATH	"BBUS_SOCKPATH"

/**
 * @}
 *
 * @defgroup __error__ Error handling
 * @{
 *
 * Error handling functions and macros.
 *
 * @defgroup __errcodes__ Error codes
 * @{
 *
 * All error codes, that can be set by busybus API functions.
 */

#define BBUS_ESUCCESS		10000 /**< No error */
#define BBUS_ENOMEM		10001 /**< Out of memory */
#define BBUS_EINVALARG		10002 /**< Invalid argument */
#define BBUS_EOBJINVFMT		10003 /**< Invalid format of an object */
#define BBUS_ENOSPACE		10004 /**< No space in buffer */
#define BBUS_ECONNCLOSED	10005 /**< Connection closed */
#define BBUS_EMSGINVFMT		10006 /**< Invalid message format */
#define BBUS_EMSGMAGIC		10007 /**< Invalid magic number in a message */
#define BBUS_EMSGINVTYPRCVD	10008 /**< Invalid message type */
#define BBUS_ESORJCTD		10009 /**< Session open rejected */
#define BBUS_ESENTLESS		10010 /**< Sent less data, than expected */
#define BBUS_ERCVDLESS		10011 /**< Received less data, than expected */
#define BBUS_ELOGICERR		10012 /**< Logic error */
#define BBUS_ENOMETHOD		10013 /**< No method with given name */
#define BBUS_EMETHODERR		10014 /**< Error calling method */
#define BBUS_EPOLLINTR		10015 /**< Poll interrupted by a signal. */
#define BBUS_EMREGERR		10016 /**< Error registering the method. */
#define BBUS_EHMAPINVTYPE	10017 /**< Invalid key type for this map. */
#define BBUS_EREGEXPTRN		10018 /**< Invalid regex pattern. */
#define BBUS_ECLIUNAUTH		10019 /**< Client unauthorized. */
#define __BBUS_MAX_ERR		10020 /**< Highest error code */

/**
 * @}
 */

/**
 * @brief Returns the value of the last error in the busybus library.
 * @return Error number.
 *
 * All functions in the busybus public API set the externally invisible error
 * value to indicate the error's cause. This function is thread-safe - it
 * returns the last error in current thread.
 */
int bbus_lasterror(void) BBUS_PUBLIC;

/**
 * @brief Returns a string representation of an error code.
 * @param errnum Error code to interpret.
 * @return Pointer to a human-readable error message.
 *
 * Returns pointer to a static string, which must not be modified. Subsequent
 * calls will not modify the string under this address. If errnum is equal
 * to one of the glibc errnos it will use strerror to obtain the
 * error message.
 */
const char* bbus_strerror(int errnum) BBUS_PUBLIC;

/**
 * @}
 *
 * @defgroup __marshalling__ Data marshalling
 * @{
 *
 * Functions and data structures for data marshalling.
 *
 * @defgroup __descr__ Marshalled data description
 * @{
 *
 * Characters used to describe the contents of busybus objects.
 */

#define BBUS_TYPE_INT32		'i'	/**< 32 bit signed integer type. */
#define BBUS_TYPE_UINT32	'u'	/**< 32 bit unsigned integer type. */
#define BBUS_TYPE_BYTE		'b'	/**< 8 bit unsigned char type. */
#define BBUS_TYPE_STRING	's'	/**< NULL-terminated string. */
#define BBUS_TYPE_ARRAY		'A'	/**< An array. */
#define BBUS_TYPE_STRUCT_START	'('	/**< Start of a struct definition. */
#define BBUS_TYPE_STRUCT_END	')'	/**< End of a struct definition. */

/**
 * @}
 *
 * @defgroup __types__ Busybus data types
 * @{
 *
 * Basic data types that can be marshalled using the object API.
 */

typedef int32_t		bbus_int32;	/**< 32 bit signed integer type. */
typedef uint32_t	bbus_uint32;	/**< 32 bit unsigned integer type. */
typedef uint32_t	bbus_size;	/**< 32 bit unsigned integer type. */
typedef uint8_t		bbus_byte;	/**< 8 bit unsigned char type. */

/**
 * @}
 */

/**
 * @brief Opaque type representing a single object containing marshalled data.
 */
typedef struct __bbus_object bbus_object;

/**
 * @brief Allocate an empty busybus object.
 * @return Pointer to a new object or NULL if no memory.
 */
bbus_object* bbus_obj_alloc(void) BBUS_PUBLIC;

/**
 * @brief Free an object.
 * @param obj The object - can be NULL.
 */
void bbus_obj_free(bbus_object* obj) BBUS_PUBLIC;

/**
 * @brief Resets the state of an object.
 * @param obj The object.
 *
 * Resetting means, that all data already marshalled will be invalidated and
 * the object will have the state of a newly allocated one.
 */
void bbus_obj_reset(bbus_object* obj) BBUS_PUBLIC;

/**
 * @brief Returns a pointer to the buffer containing the marshalled data.
 * @param obj The object.
 * @return Pointer to the internal buffer.
 */
void* bbus_obj_rawdata(bbus_object* obj) BBUS_PUBLIC;

/**
 * @brief Returns the size of the marshalled data.
 * @param obj The object.
 * @return Number of bytes stored within the object.
 */
size_t bbus_obj_rawsize(const bbus_object* obj) BBUS_PUBLIC;

/**
 * @brief Checks if given string properly describes a valid object.
 * @param descr The description.
 * @return 1 if 'descr' is valid, 0 otherwise.
 */
int bbus_obj_descrvalid(const char* descr) BBUS_PUBLIC;

/**
 * @brief Inserts an array definition into an object.
 * @param obj The object.
 * @param arrsize Number of elements the array will contain.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_insarray(bbus_object* obj, bbus_size arrsize) BBUS_PUBLIC;

/**
 * @brief Extracts an array definition from an object.
 * @param obj The object.
 * @param arrsize Place to store the extracted size.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_extrarray(bbus_object* obj, bbus_size* arrsize) BBUS_PUBLIC;

/**
 * @brief Inserts a 32-bit signed integer into an object.
 * @param obj The object.
 * @param val The value to insert.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_insint(bbus_object* obj, bbus_int32 val) BBUS_PUBLIC;

/**
 * @brief Extracts a 32-bit signed integer from an object.
 * @param obj The object.
 * @param val Place to store the extracted value.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_extrint(bbus_object* obj, bbus_int32* val) BBUS_PUBLIC;

/**
 * @brief Inserts a 32-bit unsigned integer into an object.
 * @param obj The object.
 * @param val The value to insert.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_insuint(bbus_object* obj, bbus_uint32 val) BBUS_PUBLIC;

/**
 * @brief Extracts a 32-bit unsigned integer from an object.
 * @param obj The object.
 * @param val Place to store the extracted value.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_extruint(bbus_object* obj, bbus_uint32* val) BBUS_PUBLIC;

/**
 * @brief Inserts a NULL-terminated string into an object.
 * @param obj The object.
 * @param val The string to insert.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_insstr(bbus_object* obj, const char* val) BBUS_PUBLIC;

/**
 * @brief Extracts a NULL-terminated string from an object.
 * @param obj The object.
 * @param val Place to store the extracted string.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_extrstr(bbus_object* obj, char** val) BBUS_PUBLIC;

/**
 * @brief Inserts a single byte into an object.
 * @param obj The object.
 * @param val The value to insert.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_insbyte(bbus_object* obj, bbus_byte val) BBUS_PUBLIC;

/**
 * @brief Extracts a single byte from an object.
 * @param obj The object.
 * @param val Place to store the extracted value.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_extrbyte(bbus_object* obj, bbus_byte* val) BBUS_PUBLIC;

/**
 * @brief Inserts a byte-array into an object.
 * @param obj The object.
 * @param buf The byte array to insert.
 * @param size Number of bytes to store.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_insbytes(bbus_object* obj, const void* buf,
		size_t size) BBUS_PUBLIC;

/**
 * @brief Extracts a byte-array from an object.
 * @param obj The object.
 * @param buf Place to store extracted data.
 * @param size Number of bytes to extract.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_extrbytes(bbus_object* obj, void* buf, size_t size) BBUS_PUBLIC;

/**
 * @brief Resets the extraction state of an object.
 * @param obj The object.
 *
 * Busybus objects use internal state pointers to store the position of
 * last extraction - this function resets it to the beginning of the buffer
 * allowing the data to be extracted multiple times.
 */
void bbus_obj_rewind(bbus_object* obj) BBUS_PUBLIC;

/**
 * @brief Creates an object from data stored in given buffer.
 * @param buf The buffer.
 * @param bufsize Size of the buffer.
 * @return New object or NULL if no memory.
 */
bbus_object* bbus_obj_frombuf(const void* buf, size_t bufsize) BBUS_PUBLIC;

/**
 * @brief Builds an object according to given description and arguments.
 * @param descr Valid object description.
 * @return New object or NULL on error.
 */
bbus_object* bbus_obj_build(const char* descr, ...) BBUS_PUBLIC;

/**
 * @brief Builds an object according to given description and argument list.
 * @param descr Valid object description.
 * @param va List of variadic arguments corresponding with 'descr'.
 * @return New object or NULL on error.
 */
bbus_object* bbus_obj_vbuild(const char* descr, va_list va) BBUS_PUBLIC;

/**
 * @brief Extracts all data from an object according to given description.
 * @param obj The object.
 * @param descr Valid object description.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_parse(bbus_object* obj, const char* descr, ...) BBUS_PUBLIC;

/**
 * @brief Extracts all data from an object according to given description.
 * @param obj The object.
 * @param descr Valid object description.
 * @param va List of data pointers corresponding with 'descr'.
 * @return 0 on success, -1 on error.
 */
int bbus_obj_vparse(bbus_object* obj, const char* descr,
		va_list va) BBUS_PUBLIC;

/**
 * @brief Tries to convert an object into a human-readable form.
 * @param obj The object.
 * @param descr Valid busybus object description.
 * @param buf The buffer to store the converted object in.
 * @param bufsize Size of 'buf'.
 * @return 0 if the object has been properly stored in 'buf', -1 on error.
 */
int bbus_obj_repr(bbus_object* obj, const char* descr, char* buf,
		size_t bufsize) BBUS_PUBLIC;

/**
 * @}
 *
 * @defgroup __protocol__ Protocol definitions
 * @{
 *
 * Contains data structure definitions and constants used in the busybus
 * message exchange protocol.
 */

/* TODO Signals and simple messages will be added in the future. */

#define BBUS_MAGIC	"\xBB\xC5"	/**< Busybus magic number. */
#define BBUS_MAGIC_SIZE	2		/**< Size of the magic number. */

/* TODO should go in /var/run/bbus/ in the future. */
#define BBUS_PROT_DEFSOCKPATH "/tmp/bbus.sock" /**< Default socket path. */

/**
 * @defgroup __protmsgtypes__ Protocol message types
 * @{
 *
 * Message types carried in the 'msgtype' field of the header.
 */
#define BBUS_MSGTYPE_SO		0x01 /**< Session open. */
#define BBUS_MSGTYPE_SOOK	0x02 /**< Session open confirmed. */
#define BBUS_MSGTYPE_SORJCT	0x03 /**< Session open rejected. */
#define BBUS_MSGTYPE_SRVREG	0x04 /**< Register service. */
#define BBUS_MSGTYPE_SRVUNREG	0x05 /**< Unregister service. */
#define BBUS_MSGTYPE_SRVACK	0x06 /**< Service registered (or error). */
#define BBUS_MSGTYPE_CLICALL	0x07 /**< Client calls a method. */
#define BBUS_MSGTYPE_CLIREPLY	0x08 /**< Server replies to a client. */
#define BBUS_MSGTYPE_CLISIG	0x09 /**< Client emits a signal. */
#define BBUS_MSGTYPE_SRVCALL	0x0A /**< Server calls a registered method. */
#define BBUS_MSGTYPE_SRVREPLY	0x0B /**< Method provider replies. */
#define BBUS_MSGTYPE_SRVSIG	0x0C /**< Server sends a signal to handlers. */
#define BBUS_MSGTYPE_CLOSE	0x0D /**< Client closes session. */
#define BBUS_MSGTYPE_CTRL	0x0E /**< Control message. */
#define BBUS_MSGTYPE_MON	0x0F /**< Monitoring message. */
/**
 * @}
 *
 * @defgroup __protsotypes__ Session open message types
 * @{
 *
 * These constants indicate the type of the client trying to connect
 * to the busybus daemon.
 */
#define BBUS_SOTYPE_NONE	0x00 /**< Not a session open message. */
#define BBUS_SOTYPE_MTHCL	0x01 /**< Method caller. */
#define BBUS_SOTYPE_SRVPRV	0x02 /**< Service provider. */
#define BBUS_SOTYPE_MON		0x03 /**< Message monitor. */
#define BBUS_SOTYPE_CTL		0x04 /**< Control program. */
/**
 * @}
 *
 * @defgroup __proterrcodes__ Protocol error codes
 * @{
 *
 * Error codes are used to verify, that a method has been properly called.
 * These codes are carried in the 'errcode' field of the header and matter
 * only for replies.
 */
#define BBUS_PROT_EGOOD		0x00 /**< Success. */
#define BBUS_PROT_ENOMETHOD	0x01 /**< No such method. */
#define BBUS_PROT_EMETHODERR	0x02 /**< Error calling the method. */
#define BBUS_PROT_EMREGERR	0x03 /**< Error registering the method. */
/**
 * @}
 *
 * @defgroup __protflags__ Protocol flags
 * @{
 *
 * Flags, that can be carried by the message header in the 'flags' field.
 */
#define BBUS_PROT_HASMETA	(1 << 0) /**< Message contains metadata. */
#define BBUS_PROT_HASOBJECT	(1 << 1) /**< Message contains an object. */
/**
 * @}
 */

/**
 * @brief Represents the header of every busybus message.
 */
struct bbus_msg_hdr
{
	uint16_t magic;		/**< Busybus magic number. */
	uint8_t msgtype;	/**< Message type. */
	uint8_t sotype;		/**< Session open client type. */
	uint8_t errcode;	/**< Protocol error code. */
	uint32_t token;		/**< Used only for method calling. */
	uint16_t psize;		/**< Size of the payload. */
	uint8_t flags;		/**< Various protocol flags. */
};

/**
 * @brief Number of fields in the header.
 */
#define BBUS_MSGHDR_NUMFIELDS	7

/**
 * @brief Size of the busybus message header structure.
 */
#define BBUS_MSGHDR_SIZE	(sizeof(struct bbus_msg_hdr))

/**
 * @brief Real size of the busybus message header - without any padding space.
 */
#define BBUS_MSGHDR_REALSIZE						\
	(4*sizeof(uint8_t) + 2*sizeof(uint16_t) + sizeof(uint32_t))

/**
 * @brief Biggest allowed payload size.
 */
#define BBUS_MAXPLOADSIZE	4096

/**
 * @brief Biggest allowed message size.
 */
#define BBUS_MAXMSGSIZE		(BBUS_MSGHDR_SIZE + BBUS_MAXPLOADSIZE)

/**
 * @brief Represents a busybus message.
 */
struct bbus_msg
{
	struct bbus_msg_hdr hdr;	/**< Message header. */
	char payload[1];		/**< Start of the payload data. */
};

/**
 * @brief Extracts the busybus object from the message buffer.
 * @param msg The message.
 * @return Extracted busybus object or NULL if object not present.
 *
 * The returned object has to be freed using bbus_obj_free.
 */
bbus_object* bbus_prot_extractobj(const struct bbus_msg* msg) BBUS_PUBLIC;

/**
 * @brief Extracts the meta string from the message buffer.
 * @param msg The message.
 * @return Pointer to the meta string or NULL if meta not present.
 *
 * The returned pointer points to the area inside 'msg'.
 */
const char* bbus_prot_extractmeta(const struct bbus_msg* msg) BBUS_PUBLIC;

#define BBUS_PROT_SOCKPATHMAX 256 /**< Maximum size of busybus socket path. */

/**
 * @brief Sets new busybus unix socket path.
 * @param path New path.
 *
 * This function is thread-safe.
 */
void bbus_prot_setsockpath(const char* path) BBUS_PUBLIC;

/**
 * @brief Returns current busybus unix socket path.
 * @return Current path.
 *
 * This function is thread-safe.
 */
const char* bbus_prot_getsockpath(void) BBUS_PUBLIC;

/**
 * @defgroup __header__ Header structure manipulation
 * @{
 *
 * Functions and macros serving as easy setters and getters for the
 * struct bbus_msg_hdr. These are here because bytes in the header are
 * in network byte order and writing and reading them without this
 * interface can be cumbersome.
 */

/**
 * @brief Fills some basic fields in the busybus message header.
 * @param hdr The header.
 * @param typ Type of the message.
 * @param err Error code.
 */
void bbus_hdr_build(struct bbus_msg_hdr* hdr, int typ, int err) BBUS_PUBLIC;

/**
 * @brief Returns the token value from the header in host byte order.
 * @param hdr The header.
 * @return The token.
 */
unsigned bbus_hdr_gettoken(const struct bbus_msg_hdr* hdr) BBUS_PUBLIC;

/**
 * @brief Converts given token to network byte order and assigns it to 'hdr'.
 * @param hdr The header.
 * @param tok The token.
 */
void bbus_hdr_settoken(struct bbus_msg_hdr* hdr, unsigned tok) BBUS_PUBLIC;

/**
 * @brief Returns the payload size from the header in host byte order.
 * @param hdr The header.
 * @return The payload size.
 */
size_t bbus_hdr_getpsize(const struct bbus_msg_hdr* hdr) BBUS_PUBLIC;

/**
 * @brief Converts given size to network byte order and assigns it to 'hdr'.
 * @param hdr The header.
 * @param size New size.
 *
 * If 'size' exceeds UINT16_MAX, the size will be set exactly to UINT16_MAX.
 */
void bbus_hdr_setpsize(struct bbus_msg_hdr* hdr, size_t size) BBUS_PUBLIC;

/**
 * @brief Returns true if FLAG is set in the header's flags field.
 * @param HDR The header.
 * @param FLAG The flag to be checked.
 */
#define BBUS_HDR_ISFLAGSET(HDR, FLAG) ((HDR)->flags & (FLAG))

/**
 * @brief Sets FLAG to true in the header's flags field.
 * @param HDR The header.
 * @param FLAG The flag to be set.
 */
#define BBUS_HDR_SETFLAG(HDR, FLAG)					\
	do {								\
		(HDR)->flags |= (FLAG);					\
	} while (0)

/**
 * @}
 *
 * @}
 *
 * @defgroup __caller__ Client calls
 * @{
 *
 * Functions used by method calling clients.
 */

/* TODO Asynchronous calls. */

/**
 * @brief Opaque type representing a client connection.
 */
typedef struct __bbus_client_connection bbus_client_connection;

/**
 * @brief Establishes a client connection with the busybus server.
 * @param name Name by which the client wants to identify itself.
 * @return New connection object or NULL in case of an error.
 */
bbus_client_connection* bbus_connect(const char* name) BBUS_PUBLIC;

/**
 * @brief Calls a method synchronously.
 * @param conn The client connection.
 * @param method Full service and method name.
 * @param arg Marshalled arguments.
 * @return Returned marshalled data or NULL if error.
 */
bbus_object* bbus_callmethod(bbus_client_connection* conn,
		const char* method, bbus_object* arg) BBUS_PUBLIC;

/**
 * @brief Emits a signal.
 * @param conn The client connection.
 * @param signame Full signal path.
 * @param obj Marshalled arguments.
 * @return 0 on success, -1 on failure.
 */
int bbus_emitsignal(bbus_client_connection* conn,
		const char* signame, bbus_object* obj) BBUS_PUBLIC;

/**
 * @brief Closes the client connection.
 * @param conn The client connection to close.
 * @return 0 if the connection has been properly closed, -1 on error.
 */
int bbus_closeconn(bbus_client_connection* conn) BBUS_PUBLIC;

/**
 * @defgroup __monctl__ Control and monitoring
 * @{
 *
 * Functions used to send control messages and receive monitoring info.
 */

/**
 * @brief Establishes a monitoring connection with busybus daemon.
 * @return New connection object or NULL on error.
 *
 * Monitors receive every message that has been successfully received
 * or sent by the busybus server daemon marshalled in a busybus object.
 *
 * Monitors use regular client connection objects and can be closed
 * using bbus_closeconn().
 */
bbus_client_connection* bbus_mon_connect(void) BBUS_PUBLIC;

/**
 * @brief Receive a monitoring message from the busybus daemon.
 * @param conn The monitor client connection.
 * @param msg Buffer used to store the received message.
 * @param bufsize Size of the buffer.
 * @param tv Maximum interval that this function should wait for data.
 * @param obj Received object is stored at this address.
 * @param meta The meta string is stored at this address if present.
 * @return -1 on error, 0 on timeout, 1 when a message has been received.
 */
int bbus_mon_recvmsg(bbus_client_connection* conn,
		struct bbus_msg* msg, size_t bufsize, struct bbus_timeval* tv,
		const char** meta, bbus_object** obj) BBUS_PUBLIC;

/**
 * @}
 *
 * @}
 *
 * @defgroup __service__ Service publishing
 * @{
 *
 * Functions and data structures used by service publishing clients.
 */

 /**
  * @brief Opaque type representing a service publisher connection.
  */
typedef struct __bbus_service_connection bbus_service_connection;

/**
 * @brief Represents a function that is actually being called on method call.
 */
typedef bbus_object* (*bbus_method_func)(bbus_object*);

/**
 * @brief Represents a single busybus method.
 *
 * Contains all the data, that is needed to properly register a method
 * within bbusd.
 */
struct bbus_method
{
	char* name;		/**< Name of the method. */
	char* argdscr;		/**< Description of required arguments. */
	char* retdscr;		/**< Description of the return value. */
	bbus_method_func func;	/**< Pointer to the method function. */
};

/**
 * @brief Establishes a service publisher connection with the busybus server.
 * @param name Whole path of the service location ie. 'foo.bar.baz'.
 * @return New connection object or NULL in case of an error.
 */
bbus_service_connection* bbus_srvc_connect(const char* name) BBUS_PUBLIC;

/**
 * @brief Registers a method within the busybus server.
 * @param conn The publisher connection.
 * @param method Method data to register.
 * @return 0 on successful registration, -1 on error.
 */
int bbus_srvc_regmethod(bbus_service_connection* conn,
		struct bbus_method* method) BBUS_PUBLIC;

/**
 * @brief Unregisters a method from the busybus server.
 * @param conn The publisher connection.
 * @param method Method data to unregister.
 * @return 0 on successful unregistration, -1 on error.
 */
int bbus_srvc_unregmethod(bbus_service_connection* conn,
		const char* method) BBUS_PUBLIC;

/**
 * @brief Closes the service publisher connection.
 * @param conn The publisher connection to close.
 * @return 0 if the connection has been properly closed, -1 on error.
 */
int bbus_srvc_closeconn(bbus_service_connection* conn) BBUS_PUBLIC;

/**
 * @brief Listens for method calls on an open connection.
 * @param conn The service publisher connection.
 * @param tv Time after which the function will exit with a timeout status.
 * @return An integer indicating the result.
 *
 * Returns 0 if timed out with no method call, -1 in case of an
 * error and 1 if method has been called.
 */
int bbus_srvc_listencalls(bbus_service_connection* conn,
		struct bbus_timeval* tv) BBUS_PUBLIC;

/* TODO Listening on multiple connections. */

/**
 * @}
 *
 * @defgroup __server__ Server interface
 * @{
 *
 * The busybus server API contains functions providing an interface to
 * the library's internals for the server implementation.
 *
 * @defgroup __srv_cliobj__ Client representation
 * @{
 *
 * Functions and data structures representing clients within the bbusd.
 */

/**
 * @brief Opaque object corresponding with a single connected client.
 *
 * Can be both a method-calling client and a service offering client.
 */
typedef struct __bbus_client bbus_client;

#define BBUS_CLIENT_CALLER	1 /**< Method calling client. */
#define BBUS_CLIENT_SERVICE	2 /**< Service provider. */
#define BBUS_CLIENT_MON		3 /**< Busybus monitor. */
#define BBUS_CLIENT_CTL		4 /**< Busybus control program. */

/**
 * @brief Stores the unix credentials of the client process.
 */
struct bbus_client_cred
{
	pid_t pid;	/**< Process ID of the client process */
	uid_t uid;	/**< User ID of the client process */
	gid_t gid;	/**< Group ID of the client process */
};

/**
 * @brief Gives access to the token of a bbus_client object.
 * @param cli The client.
 * @return The token.
 */
uint32_t bbus_client_gettoken(bbus_client* cli) BBUS_PUBLIC;

/**
 * @brief Assigns a new token value within a bbus_client object.
 * @param cli The client.
 * @param tok New token.
 */
void bbus_client_settoken(bbus_client* cli, uint32_t tok) BBUS_PUBLIC;

/**
 * @brief Returns the internal client type of a bbus_client object.
 * @param cli The client.
 * @return One of the possible client types.
 */
int bbus_client_gettype(bbus_client* cli) BBUS_PUBLIC;

/**
 * @brief Max size of the client name.
 */
#define BBUS_CLIENT_MAXNAMESIZE		32

/**
 * @brief Returns the name by which the client wants to be identified.
 * @return Name of the client.
 */
const char* bbus_client_getname(bbus_client* cli) BBUS_PUBLIC;

/**
 * @brief Receive a full message from client.
 * @param cli The client.
 * @param buf Buffer for the message to be stored in.
 * @param bufsize Size of 'buf'.
 * @return 0 on success, -1 on error.
 */
int bbus_client_rcvmsg(bbus_client* cli, struct bbus_msg* buf,
		size_t bufsize) BBUS_PUBLIC;

/**
 * @brief Send a full message to the client.
 * @param cli The client.
 * @param hdr Header of the message to send.
 * @param meta Meta data of the message (can be NULL).
 * @param obj Marshalled data to send (can be NULL).
 * @return 0 if a full message has been properly sent, -1 on error.
 */
int bbus_client_sendmsg(bbus_client* cli, struct bbus_msg_hdr* hdr,
		const char* meta, bbus_object* obj) BBUS_PUBLIC;

/**
 * @brief Closes the client connection.
 * @param cli The client.
 * @return 0 on success, -1 on error.
 */
int bbus_client_close(bbus_client* cli) BBUS_PUBLIC;

/**
 * @brief Frees the client object.
 * @param cli The client.
 */
void bbus_client_free(bbus_client* cli) BBUS_PUBLIC;

/**
 * @}
 */

/**
 * @brief Opaque server object.
 */
typedef struct __bbus_server bbus_server;

/**
 * @brief Creates a server instance.
 * @return Pointer to the newly created server instance or NULL on error.
 */
bbus_server* bbus_srv_create(void) BBUS_PUBLIC;

/**
 * @brief Sets the server into listening mode.
 * @param srv The server.
 * @return 0 on success, -1 on error.
 */
int bbus_srv_listen(bbus_server* srv) BBUS_PUBLIC;

/**
 * @brief Indicates whether there are pending connections on the server socket.
 * @param srv The server.
 * @return 1 if there are connections incoming, 0 if not, -1 on error.
 */
int bbus_srv_clientpending(bbus_server* srv) BBUS_PUBLIC;

#define BBUS_SRV_AUTHOK		0	/**< Client authorized. */
#define BBUS_SRV_AUTHERR	-1	/**< Client unauthorized to connect. */

/**
 * @brief Callback function used to authenticate the client before connection.
 */
typedef int (*bbus_accept_authfunc)(const struct bbus_client_cred*);

/**
 * @brief Callback called when a message is received inside bbus_srv_accept.
 */
typedef void (*bbus_accept_msgrcvdfunc)(const struct bbus_msg*);

/**
 * @brief Callback called when a message is sent inside bbus_srv_accept.
 */
typedef void (*bbus_accept_msgsentfunc)(const struct bbus_msg_hdr*,
						const char*, bbus_object*);

/**
 * @brief Contains all three available callbacks for bbus_srv_accept;
 */
struct bbus_accept_callbacks
{
	bbus_accept_authfunc auth;	/**< Authentication function. */
	bbus_accept_msgrcvdfunc rcvd;	/**< Called on message reception. */
	bbus_accept_msgsentfunc sent;	/**< Called on message send. */
};

/**
 * @brief Accepts a client connection.
 * @param srv The server.
 * @param funcs Callback functions.
 * @return New client connection or NULL on error.
 *
 * After an initial connection is established this function will call
 * 'authfunc' in order to get an authentication for the client. This function
 * will receive the unix credentials of the client process and should decide
 * whether this client is allowed to connect to the busybus daemon returning
 * BBUS_SRV_AUTHOK if so and BBUS_SRV_AUTHERR otherwise.
 *
 * The 'authfunc' pointer can be NULL - in that case no authentication will
 * be performed and every client will be accepted.
 */
bbus_client* bbus_srv_accept(bbus_server* srv,
		const struct bbus_accept_callbacks* funcs) BBUS_PUBLIC;

/**
 * @brief Stops listening on a server socket and closes it.
 * @param srv The server.
 * @return 0 on success, -1 on error.
 */
int bbus_srv_close(bbus_server* srv) BBUS_PUBLIC;

/**
 * @brief Frees the server object.
 * @param srv The server.
 */
void bbus_srv_free(bbus_server* srv) BBUS_PUBLIC;

/**
 * @defgroup __src_poll__ Polling interface
 * @{
 *
 * Set of functions and data structures allowing for easy polling for events
 * on multiple server and client objects. Used by the busybus daemon
 * implementation to limit it to a single thread only.
 */

/**
 * @brief Opaque pollset object.
 *
 * Stores server and client objects in a form suitable for polling.
 */
typedef struct __bbus_pollset bbus_pollset;

/**
 * @brief Creates and empty pollset object.
 * @return Pointer to a new pollset object or NULL if no memory.
 */
bbus_pollset* bbus_pollset_make(void) BBUS_PUBLIC;

/**
 * @brief Clears an existing pollset object.
 * @param pset The pollset.
 */
void bbus_pollset_clear(bbus_pollset* pset) BBUS_PUBLIC;

/**
 * @brief Adds a server object to the pollset.
 * @param pset The pollset.
 * @param src The server.
 */
void bbus_pollset_addsrv(bbus_pollset* pset, bbus_server* src) BBUS_PUBLIC;

/**
 * @brief Adds a client to the pollset.
 * @param pset The pollset.
 * @param cli The client.
 */
void bbus_pollset_addcli(bbus_pollset* pset, bbus_client* cli) BBUS_PUBLIC;

/**
 * @brief Performs an I/O poll on all the objects set within 'pset'.
 * @param pset The pollset.
 * @param tv Time value that is a struct bbus_timeval.
 * @return Number of descriptors ready for I/O, 0 on timeout or -1 on error.
 *
 * Checks only whether there are descriptors ready for reading.
 */
int bbus_poll(bbus_pollset* pset, struct bbus_timeval* tv) BBUS_PUBLIC;

/**
 * @brief Checks whether a server object whithin this pollset is ready for I/O.
 * @param pset The pollset.
 * @param srv The server.
 * @return 1 if the server is ready, 0 otherwise.
 */
int bbus_pollset_srvisset(bbus_pollset* pset, bbus_server* srv) BBUS_PUBLIC;

/**
 * @brief Checks whether a client object whithin this pollset is ready for I/O.
 * @param pset The pollset.
 * @param cli The client.
 * @return 1 if the client is ready, 0 otherwise.
 */
int bbus_pollset_cliisset(bbus_pollset* pset, bbus_client* cli) BBUS_PUBLIC;

/**
 * @brief Disposes of a pollset object.
 * @param pset The pollset to free.
 */
void bbus_pollset_free(bbus_pollset* pset) BBUS_PUBLIC;

/**
 * @}
 *
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __BUSYBUS__ */

