#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <conio.h>

#include <stdio.h>

#include <liuzianglib.h>
#include <DC_File.h>

static const std::string SOI{ char(0xFF), char(0xD8) }; // Start of image segment(SOI)
static const std::string App0Marker = { char(0xFF), char(0xE0) }; // APP0 marker
static const std::string App1Marker = { char(0xFF), char(0xE1) }; // APP1 marker
static const std::string App2Marker = { char(0xFF), char(0xE2) }; // APP2 marker
static const std::string JFIFIdentify = { 0x4A, 0x46, 0x49, 0x46, 0x00 }; // "JFIF\0"
static const std::string ICC_PROFILE_Identify = { 0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4f, 0x46, 0x49, 0x4C, 0x45, 0x00, 0x01, 0x01 }; // "ICC_PROFILE\0"
static int SOIandApp0SizeWithoutThumbnail = SOI.size() + App0Marker.size() + 16; // SOI + APP0 segment size without Thumbnail
static int SegmentLengthSize = 2; // Segment size is 2 bytes

bool isLittleEndian() {
	int i = 0x11223344;
	char c = *reinterpret_cast<char*>(&i);

	if (c == 0x44)
		return true;
	else
		return false;
}

short toInt16(const unsigned char *tail, int index) {
	if (isLittleEndian())
		return static_cast<short>(static_cast<unsigned short>(tail[index + 1] << 8) + static_cast<unsigned char>(tail[index]));
	else
		return static_cast<short>(static_cast<unsigned short>(tail[index] << 8) + static_cast<unsigned char>(tail[index + 1]));
}

uint32_t getExpectedFileSize(uint32_t image_size, uint32_t ICCProfile_size) {
	return image_size + App2Marker.size() + SegmentLengthSize + ICC_PROFILE_Identify.size() + ICCProfile_size;
}

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

int main(int argc, char **argv)
{
	{
		auto cmd = DC::GetCommandLineParameters(argc, argv);
		if (cmd.size() < 3) {
			std::cout << "Usage: EmbedICCProfile xxx.jpg xxx.icc";
			return 0;
		}
		if (!DC::File::exists(cmd.at(1))) {
			std::cout << "File " + cmd.at(1) + " not found";
			return 0;
		}
		if (!DC::File::exists(cmd.at(2))) {
			std::cout << "File " + cmd.at(2) + " not found";
			return 0;
		}

		auto icc = DC::File::read<DC::File::binary>(cmd.at(2));

		auto real_size = DC::File::getSize(cmd.at(1));
		auto reserve_size = getExpectedFileSize(real_size, icc.size());
		std::unique_ptr<unsigned char[]>  jpg(new unsigned char[reserve_size]);
		DC::File::file_ptr fp(fopen(cmd.at(1).c_str(), "rb"));
		if (fp.get() == 0) {
			std::cout << "Can not open " + cmd.at(1);
			return 0;
		}
		if (fread(jpg.get(), sizeof(char), reserve_size, fp.get()) != real_size)
			abort();
		fp.reset(0);

		int outsize = 0;
		if (!EmbedICCProfile(jpg.get(), real_size, reserve_size, reinterpret_cast<const unsigned char *>(icc.data()), icc.size(), &outsize)) {
			std::cout << "EmbedICCProfile failed";
			return 0;
		}
		if (outsize == 0) {
			std::cout << "EmbedICCProfile failed";
			return 0;
		}

		fp = fopen(cmd.at(1).c_str(), "wb");
		fwrite(jpg.get(), sizeof(char), outsize, fp.get());
	}
	_CrtDumpMemoryLeaks();
}