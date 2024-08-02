#include "../stereokit.h"
#include <stdint.h>
#include <stdbool.h>

namespace sk {

void* ktx2_decode  (void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count);
bool  ktx2_info    (void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count);
void* basisu_decode(void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count);
bool  basisu_info  (void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count);

}