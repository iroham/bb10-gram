/* nl_types.h for BB10 (QNX has no message catalogs): libc++'s <locale> wants the declarations. Nothing is ever
 * found, so callers get their default text. */
#ifndef BQ_NL_TYPES_H
#define BQ_NL_TYPES_H
#ifdef __cplusplus
extern "C" {
#endif
typedef void *nl_catd;
typedef int nl_item;
#define NL_SETD 1
#define NL_CAT_LOCALE 1
static inline nl_catd catopen(const char *name, int flag) { (void)name; (void)flag; return (nl_catd)-1; }
static inline char *catgets(nl_catd c, int set, int num, const char *dflt) { (void)c; (void)set; (void)num; return (char *)dflt; }
static inline int catclose(nl_catd c) { (void)c; return 0; }
#ifdef __cplusplus
}
#endif
#endif
