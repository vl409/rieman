#include <xcb/randr.h>

int main()
{
    (void) xcb_randr_get_screen_resources(NULL, 0);
    return 0;
}
