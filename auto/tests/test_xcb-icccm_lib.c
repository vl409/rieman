#include <xcb/xcb_icccm.h>

int main()
{
    (void) xcb_icccm_set_wm_hints(NULL, 0, NULL);
    return 0;
}
