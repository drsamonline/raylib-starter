/* tools/x11_stubs.c — six X11 symbols, so raylib's static library can be LINKED
 * on a machine with no X11 (this sandbox, and any CI runner without a desktop).
 *
 * This is a measurement aid, not a port: a binary linked with these cannot open a
 * window. It exists so `make size-real` can answer one question honestly - what
 * does the game plus the real raylib weigh - without a GPU or an X server.
 *
 * Each returns a null/zero answer, which is what a failed X call looks like.
 * They are WEAK, so on a machine with a real libX11 the real symbols win and
 * including this file is harmless.
 */
#include <stddef.h>

typedef struct _XDisplay Display;
typedef unsigned long Atom;
typedef unsigned long Window;

__attribute__((weak)) int   XFree(void *data) { (void)data; return 1; }
__attribute__((weak)) int   XSync(Display *d, int discard) { (void)d; (void)discard; return 1; }
__attribute__((weak)) int   XCheckTypedEvent(Display *d, int type, void *event) { (void)d; (void)type; (void)event; return 0; }
__attribute__((weak)) Atom  XInternAtom(Display *d, const char *name, int only_if_exists) {
    (void)d; (void)name; (void)only_if_exists; return 0;
}
__attribute__((weak)) int   XGetWindowProperty(Display *d, Window w, Atom prop, long off, long len,
                         int del, Atom type, Atom *type_ret, int *format_ret,
                         unsigned long *n_ret, unsigned long *after, unsigned char **data) {
    (void)d; (void)w; (void)prop; (void)off; (void)len; (void)del; (void)type;
    (void)type_ret; (void)format_ret; (void)n_ret; (void)after;
    if (data) *data = NULL;
    return 0;
}
__attribute__((weak)) int   XConvertSelection(Display *d, Atom sel, Atom target, Atom prop,
                        Window req, long time) {
    (void)d; (void)sel; (void)target; (void)prop; (void)req; (void)time; return 0;
}
