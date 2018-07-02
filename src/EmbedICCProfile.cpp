#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <iostream>
#include <algorithm>
#include <memory>

#include <cstdio>
#include <cstdint>

static constexpr unsigned char soi[]{ 0xff,0xd8 };
static constexpr unsigned char app0_marker[]{ 0xff,0xe0 };
static constexpr unsigned char app1_marker[]{ 0xff,0xe1 };
static constexpr unsigned char app2_marker[]{ 0xff,0xe2 };
static constexpr unsigned char jfif_identify[]{ 0x4a, 0x46, 0x49, 0x46, 0x00 };
static constexpr unsigned char icc_profile_identify[]{ 0x49, 0x43, 0x43, 0x5f, 0x50, 0x52, 0x4f, 0x46, 0x49, 0x4c, 0x45, 0x00, 0x01, 0x01 };
static constexpr int soi_and_app0_size_without_thumbnail = sizeof(soi) + sizeof(app0_marker) + 16;
static constexpr int segment_length_size = 2;
static const bool is_little_endian = [] {
	const unsigned int i = 0x11223344;
	return *reinterpret_cast<const unsigned char*>(&i) == 0x44;
}();

inline int16_t to_int_16(const unsigned char *tail, int index) {
	if (is_little_endian)
		return static_cast<int16_t>(static_cast<uint16_t>(tail[index + 1] << 8) + static_cast<unsigned char>(tail[index]));
	else
		return static_cast<int16_t>(static_cast<uint16_t>(tail[index] << 8) + static_cast<unsigned char>(tail[index + 1]));
}

using file_ptr = std::unique_ptr<FILE, void(*)(FILE *)>;

file_ptr get_file_ptr(FILE *fp) {
	return file_ptr(fp, [](FILE *fp)->void {
		if (fp != 0 && fp != nullptr)
			fclose(fp);
	});
}

inline bool file_exists(const std::string& filename)noexcept {
	return static_cast<bool>(get_file_ptr(fopen(filename.c_str(), "r")));
}

inline bool file_exists(const std::string& filename, file_ptr& inputptr)noexcept {
	inputptr = get_file_ptr(fopen(filename.c_str(), "r"));
	return static_cast<bool>(inputptr);
}

std::size_t file_size(const std::string& filename) {
	auto ptr = get_file_ptr(fopen(filename.c_str(), "r"));
	fseek(ptr.get(), 0L, SEEK_END);
	return static_cast<std::size_t>(ftell(ptr.get()));
}

//计算图像置入色彩文件后的大小
inline uint32_t get_expected_file_size(uint32_t image_size, uint32_t icc_profile_size) {
	return image_size + sizeof(app2_marker) + segment_length_size + sizeof(icc_profile_identify) + icc_profile_size;
}

//置入色彩文件到图像
//image是图片缓冲区，image_size是图片实际大小，image_reserve_size 是图片缓冲区大小，icc_profile是色彩文件缓冲区，icc_profile_size是色彩文件大小，output_size指向的对象将会在函数执行后得到嵌入色彩文件后图片的大小
//成功返回true，失败返回false
bool EmbedICCProfile(unsigned char * const image, int32_t image_size, int32_t image_reserve_size, const unsigned char * const icc_profile, int32_t icc_profile_size, int32_t* const output_size) {
	if (icc_profile_size > 0xffff) {
		*output_size = image_size;
		return false;
	}

	std::size_t expected_file_size = image_size + sizeof(app2_marker) + segment_length_size + sizeof(icc_profile_identify) + icc_profile_size;

	if (static_cast<std::size_t>(image_reserve_size) < expected_file_size) {
		*output_size = image_size;
		return false;
	}

	unsigned char app2_segment_size_buffer[segment_length_size];
	unsigned char app0_header_size_pun[segment_length_size];
	unsigned char app0[soi_and_app0_size_without_thumbnail];
	memcpy(app0, image, soi_and_app0_size_without_thumbnail);

	unsigned char *ptr = app0 + sizeof(soi) + sizeof(app0_marker);
	for (std::size_t i = 0; i < segment_length_size; i++, ptr++)
		app0_header_size_pun[i] = *ptr;
	if (is_little_endian)
		std::reverse(app0_header_size_pun, app0_header_size_pun + segment_length_size);

	int16_t	app0_header_size = to_int_16(app0_header_size_pun, 0);
	uint32_t app2_segment_size = icc_profile_size + sizeof(icc_profile_identify) + segment_length_size;

	if (is_little_endian) {
		app2_segment_size_buffer[0] = *(reinterpret_cast<unsigned char *>(&app2_segment_size) + 1);
		app2_segment_size_buffer[1] = *(reinterpret_cast<unsigned char *>(&app2_segment_size));
	}
	else {
		memcpy(app2_segment_size_buffer, reinterpret_cast<unsigned char *>(&app2_segment_size), 2);
	}

	std::unique_ptr<unsigned char[]> jpg_image_without_header(new unsigned char[image_size]);
	memcpy(jpg_image_without_header.get(), image, image_size);

	unsigned char *ptr2 = image + sizeof(soi) + sizeof(app0_marker) + app0_header_size;
	memcpy(ptr2, app2_marker, sizeof(app2_marker));
	ptr2 += sizeof(app2_marker);
	memcpy(ptr2, app2_segment_size_buffer, segment_length_size);
	ptr2 += segment_length_size;
	memcpy(ptr2, icc_profile_identify, sizeof(icc_profile_identify));
	ptr2 += sizeof(icc_profile_identify);
	memcpy(ptr2, icc_profile, icc_profile_size);
	ptr2 += icc_profile_size;
	memcpy(ptr2, jpg_image_without_header.get() + sizeof(soi) + sizeof(app0_marker) + app0_header_size, image_size - (app0_header_size + sizeof(soi) + sizeof(app0_marker)));
	*output_size = expected_file_size;

	return true;
}

int main(int argc, char **argv) {
	{
		if (argc < 3) {
			std::cout << "Usage: EmbedICCProfile xxx.jpg xxx.icc\n";
			return 0;
		}

		if (!file_exists(argv[1])) {
			std::cout << "File " << argv[1] << " not found\n";
			return 0;
		}
		if (!file_exists(argv[2])) {
			std::cout << "File " << argv[2] << " not found\n";
			return 0;
		}

		auto jpg_size = file_size(argv[1]);
		auto icc_size = file_size(argv[2]);
		auto reserve_size = get_expected_file_size(jpg_size, icc_size);

		std::unique_ptr<unsigned char[]>  jpg(new unsigned char[reserve_size]);
		std::unique_ptr<unsigned char[]> icc(new unsigned char[icc_size]);

		auto fp = get_file_ptr(fopen(argv[1], "rb"));
		if (!fp) {
			std::cout << "Can not open " << argv[1] << "\n";
			return 0;
		}
		if (fread(jpg.get(), sizeof(unsigned char), reserve_size, fp.get()) != jpg_size) {
			std::cout << "Can not read " << argv[1] << "\n";
			return 0;
		}

		fp.reset(fopen(argv[2], "rb"));
		if (!fp) {
			std::cout << "Can not open " << argv[2] << "\n";
			return 0;
		}
		if (fread(icc.get(), sizeof(unsigned char), icc_size, fp.get()) != icc_size) {
			std::cout << "Can not read " << argv[2] << "\n";
			return 0;
		}

		fp.reset(0);

		int outsize = 0;
		if (!EmbedICCProfile(jpg.get(), jpg_size, reserve_size, icc.get(), icc_size, &outsize)) {
			std::cout << "EmbedICCProfile failed\n";
			return 0;
		}
		if (outsize == 0) {
			std::cout << "EmbedICCProfile failed\n";
			return 0;
		}

		fp.reset(fopen(argv[1], "wb"));
		if (!fp) {
			std::cout << "write file failed\n";
			return 0;
		}
		if (fwrite(jpg.get(), sizeof(unsigned char), outsize, fp.get()) != outsize) {
			std::cout << "write file failed\n";
			return 0;
		}

		return 0;
	}
#ifdef _WIN32
	_CrtDumpMemoryLeaks();
#endif
}
