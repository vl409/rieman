#include <xcb/xcb_util.h>

int main()
{
    (void) xcb_event_get_error_label(0);
    return 0;
}
