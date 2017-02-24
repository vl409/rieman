#include <xcb/xcb.h>

int main()
{
    (void) xcb_get_setup(NULL);
    return 0;
}
