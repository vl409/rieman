#include <X11/Xlib-xcb.h>
int main()
{
    (void) XGetXCBConnection(NULL);

    return 0;
}
