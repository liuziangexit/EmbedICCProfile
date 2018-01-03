//使用 CRT 库检查内存泄漏: https://msdn.microsoft.com/en-us/library/x98tx3cf.aspx
#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <stdlib.h>
#include <crtdbg.h>
#endif

//标准库
#include <iostream>
#include <string>
#include <vector>
#include <memory>

//C标准库
#include <cstdio>

static const std::string SOI{ char(0xFF), char(0xD8) }; //SOI开始
static const std::string App0Marker = { char(0xFF), char(0xE0) }; //APP0标记
static const std::string App1Marker = { char(0xFF), char(0xE1) }; //APP1标记
static const std::string App2Marker = { char(0xFF), char(0xE2) }; //APP2标记
static const std::string JFIFIdentify = { 0x4A, 0x46, 0x49, 0x46, 0x00 }; //"JFIF\0"
static const std::string ICC_PROFILE_Identify = { 0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4f, 0x46, 0x49, 0x4C, 0x45, 0x00, 0x01, 0x01 }; //"ICC_PROFILE\0"
static int SOIandApp0SizeWithoutThumbnail = SOI.size() + App0Marker.size() + 16; //SOI+APP0段长度(不包含略缩图)
static int SegmentLengthSize = 2; //段长度是2字节

								  //判断计算机字节序
								  //什么是字节序: https://zhuanlan.zhihu.com/p/21388517
bool isLittleEndian() {
	int i = 0x11223344;
	char c = *reinterpret_cast<char*>(&i);

	if (c == 0x44)
		return true;
	else
		return false;
}

//从字节数组转到Int16
short toInt16(const unsigned char *tail, int index) {
	if (isLittleEndian())
		return static_cast<short>(static_cast<unsigned short>(tail[index + 1] << 8) + static_cast<unsigned char>(tail[index]));
	else
		return static_cast<short>(static_cast<unsigned short>(tail[index] << 8) + static_cast<unsigned char>(tail[index + 1]));
}

//计算图像置入色彩文件后的大小
uint32_t getExpectedFileSize(uint32_t image_size, uint32_t ICCProfile_size) {
	return image_size + App2Marker.size() + SegmentLengthSize + ICC_PROFILE_Identify.size() + ICCProfile_size;
}

//置入色彩文件到图像(核心科技)
//image是图片缓冲区，image_size是图片实际大小，image_reserve_size 是图片缓冲区大小，ICCProfile是色彩文件缓冲区，ICCProfile_size是色彩文件大小，output_size指向的对象将会在函数执行后得到嵌入色彩文件后图片的大小
//成功返回true，失败返回false
bool EmbedICCProfile(unsigned char *image, int image_size, int image_reserve_size, const unsigned char *ICCProfile, int ICCProfile_size, int* output_size) {
	if (ICCProfile_size > 0xFFFF) {
		*output_size = image_size;
		return false;
	}

	uint32_t expectedFileSize = image_size + App2Marker.size() + SegmentLengthSize + ICC_PROFILE_Identify.size() + ICCProfile_size;

	if (image_reserve_size < expectedFileSize) {
		*output_size = image_size;
		return false;
	}

	std::unique_ptr<unsigned char[]> App0(new unsigned char[SOIandApp0SizeWithoutThumbnail]);
	memcpy(App0.get(), image, SOIandApp0SizeWithoutThumbnail);

	int App0HeaderSize;
	std::vector<unsigned char> temp;
	temp.reserve(SegmentLengthSize + 1);

	unsigned char *ptr = App0.get() + SOI.size() + App0Marker.size();
	for (uint32_t i = 0; i < SegmentLengthSize; i++, ptr++)
		temp.push_back(*ptr);

	if (isLittleEndian())
		std::reverse(temp.begin(), temp.end());

	App0HeaderSize = toInt16(temp.data(), 0);

	std::unique_ptr<unsigned char[]> App2SegmentSizeBuffer(new unsigned char[SegmentLengthSize]);
	int32_t App2SegmentSize = ICCProfile_size + ICC_PROFILE_Identify.size() + SegmentLengthSize;

	if (isLittleEndian()) {
		App2SegmentSizeBuffer[0] = *(reinterpret_cast<unsigned char *>(&App2SegmentSize) + 1);
		App2SegmentSizeBuffer[1] = *(reinterpret_cast<unsigned char *>(&App2SegmentSize));
	}
	else {
		memcpy(App2SegmentSizeBuffer.get(), reinterpret_cast<unsigned char *>(&App2SegmentSize), 2);
	}

	std::unique_ptr<unsigned char[]> msJpegImageWithoutHeader(new unsigned char[image_size]);
	memcpy(msJpegImageWithoutHeader.get(), image, image_size);

	unsigned char *ptr2 = image + SOI.size() + App0Marker.size() + App0HeaderSize;
	memcpy(ptr2, App2Marker.data(), App2Marker.size());
	ptr2 += App2Marker.size();
	memcpy(ptr2, App2SegmentSizeBuffer.get(), SegmentLengthSize);
	ptr2 += SegmentLengthSize;
	memcpy(ptr2, ICC_PROFILE_Identify.data(), ICC_PROFILE_Identify.size());
	ptr2 += ICC_PROFILE_Identify.size();
	memcpy(ptr2, ICCProfile, ICCProfile_size);
	ptr2 += ICCProfile_size;
	uint32_t JpegImageDataLength = image_size - (App0HeaderSize + SOI.size() + App0Marker.size());
	memcpy(ptr2, msJpegImageWithoutHeader.get() + SOI.size() + App0Marker.size() + App0HeaderSize, JpegImageDataLength);
	*output_size = expectedFileSize;
	return true;
}

//unique_ptr deleter
void file_ptr_deleter(FILE *fp) {
	if (fp != 0 && fp != nullptr)
		fclose(fp);
}

//保存文件指针的unique_ptr
//什么是unique_ptr: http://en.cppreference.com/w/cpp/memory/unique_ptr
using file_ptr = std::unique_ptr<FILE, decltype(&file_ptr_deleter)>;

//文件是否存在
inline bool file_exists(const std::string& filename)noexcept {
	file_ptr ptr(fopen(filename.c_str(), "r"), file_ptr_deleter);
	return (bool)ptr;
}

//文件是否存在
inline bool file_exists(const std::string& filename, file_ptr& inputptr)noexcept {
	file_ptr ptr(fopen(filename.c_str(), "r"), file_ptr_deleter);
	inputptr = std::move(ptr);
	return (bool)inputptr;
}

//获取文件长度
std::size_t file_size(const std::string& filename) {
	file_ptr ptr(0, file_ptr_deleter);
	if (!file_exists(filename, ptr))
		throw std::exception();
	if (fseek(ptr.get(), 0L, SEEK_END) != 0)
		throw std::exception();
	const auto& rv = ftell(ptr.get());
	if (rv < 0)
		throw std::exception();
	return rv;//这里的有符号无符号不匹配可以忽略
}

int main(int argc, char **argv)
{
	{
		//检查参数
		if (argc < 3) {
			std::cout << "Usage: EmbedICCProfile xxx.jpg xxx.icc";
			return 0;
		}

		//检查文件是否存在
		if (!file_exists(argv[1])) {
			std::cout << std::string("File ") + std::string(argv[1]) + " not found";
			return 0;
		}
		if (!file_exists(argv[2])) {
			std::cout << std::string("File ") + std::string(argv[2]) + " not found";
			return 0;
		}

		//获取文件长度
		auto jpg_size = file_size(argv[1]);
		auto icc_size = file_size(argv[2]);
		auto reserve_size = getExpectedFileSize(jpg_size, icc_size);

		//建立缓冲区
		std::unique_ptr<unsigned char[]>  jpg(new unsigned char[reserve_size]), icc(new unsigned char[icc_size]);

		//读取jpg图像
		file_ptr fp(fopen(argv[1], "rb"), file_ptr_deleter);
		if (!fp) {
			std::cout << "Can not open " + std::string(argv[1]);
			return 0;
		}
		if (fread(jpg.get(), sizeof(unsigned char), reserve_size, fp.get()) != jpg_size) {
			std::cout << "Can not read " + std::string(argv[1]);
			return 0;
		}

		//读取色彩配置文件
		fp.reset(fopen(argv[2], "rb"));
		if (!fp) {
			std::cout << "Can not open " + std::string(argv[2]);
			return 0;
		}
		if (fread(icc.get(), sizeof(unsigned char), icc_size, fp.get()) != icc_size) {
			std::cout << "Can not read " + std::string(argv[2]);
			return 0;
		}

		fp.reset(0);

		//嵌入色彩配置文件到图像
		int outsize = 0;
		if (!EmbedICCProfile(jpg.get(), jpg_size, reserve_size, icc.get(), icc_size, &outsize)) {
			std::cout << "EmbedICCProfile failed";
			return 0;
		}
		if (outsize == 0) {
			std::cout << "EmbedICCProfile failed";
			return 0;
		}

		//写入图像
		fp.reset(fopen(argv[1], "wb"));
		if (!fp) {
			std::cout << "write file failed";
			return 0;
		}
		if (fwrite(jpg.get(), sizeof(unsigned char), outsize, fp.get()) != outsize) {
			std::cout << "write file failed";
			return 0;
		}

	}
#ifdef _WIN32
	_CrtDumpMemoryLeaks();
#endif
}