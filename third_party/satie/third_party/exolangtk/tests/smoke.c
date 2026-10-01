#include "InteropTk.h"
#include "FFItk.h"
#include "DebugTk.h"
#include "ExtensionTk.h"

int main(void)
{
    itk_target_info target;
    itk_type integer = itk_type_prim(ITK_KIND_INT);
    etk_version version;
    char text[32];

    if (itk_target_query(&target) == 0) return 1;
    if (itk_type_size(&integer) != sizeof(int)) return 2;
    if (etk_version_parse("1.2.3", &version) != ETK_OK) return 3;
    if (etk_version_fmt(&version, text, sizeof(text)) != ETK_OK) return 4;
    return text[0] == '1' ? 0 : 5;
}
