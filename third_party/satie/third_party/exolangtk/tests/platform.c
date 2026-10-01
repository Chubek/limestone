/* Check both detected and fallback byte order against object representation. */
#define ITK_PLATFORM_IMPLEMENTATION
#include "InteropTk/itk_platform.h"
int main(void)
{
    const uint16_t word = 1;
    const unsigned char *bytes = (const unsigned char *)&word;
    itk_target_info target;
    itk_byteorder expected = bytes[0] ? ITK_BYTEORDER_LITTLE : ITK_BYTEORDER_BIG;
    return itk_target_query(&target) == NULL || target.byteorder != expected ||
           itk_byteorder_probe_() != (unsigned char)expected;
}
