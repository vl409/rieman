#include <xcb/xcb_ewmh.h>

int main()
{
    (void) xcb_ewmh_init_atoms(NULL, NULL);
    return 0;
}
