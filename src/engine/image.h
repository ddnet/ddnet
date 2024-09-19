#ifndef ENGINE_IMAGE_H
#define ENGINE_IMAGE_H

#include <cstdint>

#include <base/color.h>

/**
 * Represents an image that has been loaded into main memory.
 */
class CImageInfo
{
public:
	/**
	 * Defines the format of image data.
	 */
	enum EImageFormat
	{
		FORMAT_UNDEFINED = -1,
		FORMAT_RGB = 0,
		FORMAT_RGBA = 1,
		FORMAT_R = 2,
		FORMAT_RA = 3,
	};

	/**
	 * Width of the image.
	 */
	size_t m_Width = 0;

	/**
	 * Height of the image.
	 */
	size_t m_Height = 0;

	/**
	 * Format of the image.
	 *
	 * @see EImageFormat
	 */
	EImageFormat m_Format = FORMAT_UNDEFINED;

	/**
	 * Pointer to the image data.
	 */
	uint8_t *m_pData = nullptr;

	/**
	 * Frees the image data and clears all info.
	 */
	void Free();

	/**
	 * Returns the pixel size in bytes for the given image format.
	 *
	 * @param Format Image format, must not be `FORMAT_UNDEFINED`.
	 *
	 * @return Size of one pixel with the given image format.
	 */
	static size_t PixelSize(EImageFormat Format);

	/**
	 * Returns a readable name for the given image format.
	 *
	 * @param Format Image format.
	 *
	 * @return Readable name for the given image format.
	 */
	static const char *FormatName(EImageFormat Format);

	/**
	 * Returns the pixel size in bytes for the format of this image.
	 *
	 * @return Size of one pixel with the format of this image.
	 *
	 * @remark The format must not be `FORMAT_UNDEFINED`.
	 */
	size_t PixelSize() const;

	/**
	 * Returns a readable name for the format of this image.
	 *
	 * @return Readable name for the format of this image.
	 */
	const char *FormatName() const;

	/**
	 * Returns the size of the data, as derived from the width, height and pixel size.
	 *
	 * @return Expected size of the image data.
	 */
	size_t DataSize() const;

	/**
	 * Returns whether this image is equal to the given image
	 * in width, height, format and data.
	 *
	 * @param Other The image to compare with.
	 *
	 * @return `true` if the images are identical, `false` otherwise.
	 */
	bool DataEquals(const CImageInfo &Other) const;

	/**
	 * Returns the color of the pixel at the specified position.
	 *
	 * @param x The x-coordinate to read from.
	 * @param y The y-coordinate to read from.
	 *
	 * @return Pixel color converted to normalized RGBA.
	 */
	ColorRGBA PixelColor(size_t x, size_t y) const;

	/**
	 * Sets the color of the pixel at the specified position.
	 *
	 * @param x The x-coordinate to write to.
	 * @param y The y-coordinate to write to.
	 * @param Color The normalized RGBA color to write.
	 */
	void SetPixelColor(size_t x, size_t y, ColorRGBA Color) const;

	/**
	 * Copies a rectangle of image data from the given image to this image.
	 *
	 * @param SrcImage The image to copy data from.
	 * @param SrcX The x-offset in the source image.
	 * @param SrcY The y-offset in the source image.
	 * @param Width The width of the rectangle to copy.
	 * @param Height The height of the rectangle to copy.
	 * @param DestX The x-offset in the destination image (this).
	 * @param DestY The y-offset in the destination image (this).
	 */
	void CopyRectFrom(const CImageInfo &SrcImage, size_t SrcX, size_t SrcY, size_t Width, size_t Height, size_t DestX, size_t DestY) const;
};

#endif
