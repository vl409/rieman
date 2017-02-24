#include <fontconfig/fontconfig.h>

int main()
{
    (void) FcInitLoadConfigAndFonts();
    return 0;
}
