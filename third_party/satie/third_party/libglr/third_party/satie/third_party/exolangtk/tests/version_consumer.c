#define ETK_DEF extern
#define ITK_DEF extern
#include "ExtensionTk/etk_version.h"
int main(void)
{
    etk_version version;
    return etk_version_parse("2.1.0", &version) != ETK_OK ||
           !etk_version_satisfies(&version, ">=2.0.0 <3.0.0");
}
