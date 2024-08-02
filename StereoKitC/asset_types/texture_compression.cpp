#include "texture_compression.h"
#include "../sk_memory.h"

#include <sk_gpu.h>
#include <basisu_transcoder.h>

using namespace basist;

namespace sk {

///////////////////////////////////////////

skg_tex_fmt_ texture_preferred_compressed_format(int32_t channels, bool is_srgb) {
#if defined(SKG_DIRECT3D11)
	// D3D's block compression formats are generally pretty clear about what
	// formats are good for which purposes. Here we're skipping BC7 in favor of
	// the older BC1/BC3 format. The quality isn't as good, but the size is
	// smaller, and the complexity is lower.
	if (channels == 1) {
		return skg_tex_fmt_supported(skg_tex_fmt_bc4_r)
			? skg_tex_fmt_bc4_r
			: skg_tex_fmt_r8;
	} else if (channels == 2) {
		return skg_tex_fmt_supported(skg_tex_fmt_bc5_rg)
			? skg_tex_fmt_bc5_rg
			: skg_tex_fmt_r8g8;
	} else if (channels == 3) {
		return skg_tex_fmt_supported(skg_tex_fmt_bc1_rgb)
			? (is_srgb ? skg_tex_fmt_bc1_rgb_srgb : skg_tex_fmt_bc1_rgb)
			: (is_srgb ? skg_tex_fmt_rgba32       : skg_tex_fmt_rgba32_linear);
	} else if (channels == 4) {
		return skg_tex_fmt_supported(skg_tex_fmt_bc3_rgba)
			? (is_srgb ? skg_tex_fmt_bc3_rgba_srgb : skg_tex_fmt_bc3_rgba)
			: (is_srgb ? skg_tex_fmt_rgba32        : skg_tex_fmt_rgba32_linear);
	}
#else
	// The Adreno docs say this:
	// https://developer.qualcomm.com/sites/default/files/docs/adreno-gpu/snapdragon-game-toolkit/gdg/gpu/best_practices_texture.html#compression-strategies
	//
	// - Use ASTC compression if it is available.
	// - Otherwise, use ETC2 compression if available.
	// - Otherwise, select the compression format as follows :
	//   - ATC if using alpha
	//   - ETC if not using alpha
	if      (                 skg_tex_fmt_supported(skg_tex_fmt_atsc4x4_rgba)) return skg_tex_fmt_atsc4x4_rgba;
	else if (                 skg_tex_fmt_supported(skg_tex_fmt_etc2)) return skg_tex_fmt_etc2;
	else if (channels == 4 && skg_tex_fmt_supported(skg_tex_fmt_atc )) return skg_tex_fmt_atc;
	else if (                 skg_tex_fmt_supported(skg_tex_fmt_etc )) return skg_tex_fmt_etc;
#endif
	log_err("Shouldn't get here!");
	return skg_tex_fmt_none;
}

///////////////////////////////////////////

transcoder_texture_format texture_transcode_format(skg_tex_fmt_ format) {
	switch (format) {
	case skg_tex_fmt_rgba32:
	case skg_tex_fmt_rgba32_linear:    return transcoder_texture_format::cTFRGBA32;
	case skg_tex_fmt_bc1_rgb:
	case skg_tex_fmt_bc1_rgb_srgb:     return transcoder_texture_format::cTFBC1_RGB;
	case skg_tex_fmt_bc3_rgba:
	case skg_tex_fmt_bc3_rgba_srgb:    return transcoder_texture_format::cTFBC3_RGBA;
	case skg_tex_fmt_bc4_r:            return transcoder_texture_format::cTFBC4_R;
	case skg_tex_fmt_bc5_rg:           return transcoder_texture_format::cTFBC5_RG;
	case skg_tex_fmt_bc7_rgba:
	case skg_tex_fmt_bc7_rgba_srgb:    return transcoder_texture_format::cTFBC7_RGBA;
	case skg_tex_fmt_atc_rgb:          return transcoder_texture_format::cTFATC_RGB;
	case skg_tex_fmt_atc_rgba:         return transcoder_texture_format::cTFATC_RGBA;
	case skg_tex_fmt_astc4x4_rgba:
	case skg_tex_fmt_astc4x4_rgba_srgb:return transcoder_texture_format::cTFASTC_4x4_RGBA;
	//case skg_tex_fmt_pvrtc2_rgb:   return transcoder_texture_format::cTFPVRTC2_4_RGB;
	case skg_tex_fmt_pvrtc2_rgba:      return transcoder_texture_format::cTFPVRTC2_4_RGBA;
	case skg_tex_fmt_etc2_r11:         return transcoder_texture_format::cTFETC2_EAC_R11;
	case skg_tex_fmt_etc2_rg11:        return transcoder_texture_format::cTFETC2_EAC_RG11;
	}
	log_err("Shouldn't get here!");
	return transcoder_texture_format::cTFRGBA32;
}

///////////////////////////////////////////

bool ktx2_info(void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count) {
	ktx2_transcoder ktx_transcoder;
	if (!ktx_transcoder.init(data, (uint32_t)data_size)) return false;

	ktx2_header header = ktx_transcoder.get_header();
	*out_width       = ktx_transcoder.get_width ();
	*out_height      = ktx_transcoder.get_height();
	*out_mip_count   = ktx_transcoder.get_levels() == 0 ? 1 : ktx_transcoder.get_levels();
	*out_array_count = ktx_transcoder.get_layers() == 0 ? 1 : ktx_transcoder.get_layers();
	if (ktx_transcoder.get_faces() > 1) // If it's a cubemap, it'll have a value of 6 here, otherwise it'll be 1.
		*out_array_count = ktx_transcoder.get_faces();

	int32_t channels = ktx_transcoder.get_has_alpha() ? 4 : 3;
	bool    is_srgb  = ktx_transcoder.get_dfd_transfer_func() == KTX2_KHR_DF_TRANSFER_SRGB;
	*out_format = (tex_format_)texture_preferred_compressed_format(channels, is_srgb);
	ktx_transcoder.clear();
	return true;
}

///////////////////////////////////////////

void* ktx2_decode(void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count) {
	ktx2_transcoder ktx_transcoder;
	if (!ktx_transcoder.init(data, (uint32_t)data_size)) return nullptr;

	ktx2_header header = ktx_transcoder.get_header();
	*out_width       = ktx_transcoder.get_width ();
	*out_height      = ktx_transcoder.get_height();
	*out_mip_count   = ktx_transcoder.get_levels();
	*out_array_count = ktx_transcoder.get_layers() == 0 ? 1 : ktx_transcoder.get_layers();
	if (ktx_transcoder.get_faces() > 1) // If it's a cubemap, it'll have a value of 6 here, otherwise it'll be 1.
		*out_array_count = ktx_transcoder.get_faces();

	int32_t channels = ktx_transcoder.get_has_alpha() ? 4 : 3;
	bool    is_srgb  = ktx_transcoder.get_dfd_transfer_func() == KTX2_KHR_DF_TRANSFER_SRGB;
	*out_format = (tex_format_)texture_preferred_compressed_format(channels, is_srgb);
	transcoder_texture_format tc_fmt = texture_transcode_format((skg_tex_fmt_)*out_format);

	log_infof("Loading KTX2 with %s %s data. %dx%d * %d%s",
		is_srgb ? "srgb":"linear", 
		ktx_transcoder.get_format() == basis_tex_format::cUASTC4x4 ? "UASTC" : "ETC1S",
		*out_width,
		*out_height,
		*out_array_count,
		*out_mip_count > 1 ? " (+mips)":"");

	int32_t block_px   = skg_tex_fmt_block_px((skg_tex_fmt_)*out_format);
	int32_t layer_size = 0;
	for (int32_t mip = 0; mip < *out_mip_count; mip++) {
		int32_t mip_width, mip_height;
		skg_mip_dimensions(*out_width, *out_height, mip, &mip_width, &mip_height);
		layer_size += skg_tex_fmt_memory((skg_tex_fmt_)*out_format, mip_width, mip_height);
	}
	//int32_t top_level_size = block_width * block_height * skg_tex_fmt_block_size((skg_tex_fmt_)*out_format);
	//int32_t mipped_size    = top_level_size
	void*   result = sk_malloc(layer_size * *out_array_count);

	ktx2_transcoder_state state = {};
	ktx_transcoder.start_transcoding();
	bool    success    = true;
	int32_t mip_offset = 0;
	//for (uint32_t mip = 0; success && mip < ktx_transcoder.get_levels(); mip++) {
	for (uint32_t mip = 0; success && mip < 1; mip++) {
		int32_t mip_width, mip_height;
		skg_mip_dimensions(*out_width, *out_height, mip, &mip_width, &mip_height);
		int32_t mip_block_width  = mip_width  / block_px;
		int32_t mip_block_height = mip_height / block_px;

		int32_t layer_count = ktx_transcoder.get_layers() == 0 ? 1 : ktx_transcoder.get_layers();
		for (uint32_t layer = 0; success && layer < layer_count; layer++) {
			for (uint32_t face = 0; success && face < ktx_transcoder.get_faces(); face++) {
				int32_t layer_idx = layer + face;

				success = ktx_transcoder.transcode_image_level(mip, layer, face, ((uint8_t*)result) + layer_size*layer_idx + mip_offset, mip_block_width * mip_block_height, tc_fmt, 0, mip_block_width, mip_block_height, 0, 0, &state);
			}
		}

		mip_offset += skg_tex_fmt_memory((skg_tex_fmt_)*out_format, mip_width, mip_height);
	}
	ktx_transcoder.clear();
	return success ? result : nullptr;
}

///////////////////////////////////////////

bool basisu_info(void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count) {
	return false;
}

///////////////////////////////////////////

void* basisu_decode(void* data, size_t data_size, tex_format_* out_format, int32_t* out_width, int32_t* out_height, int32_t* out_array_count, int32_t* out_mip_count) {
	basisu_transcoder transcoder;
	if (!transcoder.validate_header(data, (uint32_t)data_size)) return nullptr;

	basisu_file_info file_info = {};
	if (!transcoder.get_file_info(data, (uint32_t)data_size, file_info))
		return nullptr;

	if (file_info.m_tex_type != cBASISTexType2D)
		return nullptr;
	file_info.m_tex_format; // cETC1S = 0, or cUASTC4x4 = 1
	file_info.m_tex_type;

	basisu_image_info image_info = {};
	if (!transcoder.get_image_info(data, (uint32_t)data_size, image_info, 0)) {
		return nullptr;
	}

	*out_width  = image_info.m_width;
	*out_height = image_info.m_height;
	// Transcode the .basis file to RGBA32
	/*if (!transcoder.transcode_image_level(basisFileData.data(), basisFileData.size(), 0, 0, decodedImage.data(), imageInfo.m_width * imageInfo.m_height, basist::transcoder_texture_format::cTFRGBA32)) {
		std::cerr << "Failed to transcode image!" << std::endl;
		return -1;
	}*/
	return nullptr;
}

}