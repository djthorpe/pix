/**
 * @file include/pix/image.h
 * @brief Image loading helpers (currently JPEG via tiny JPEG decoder).
 */
#pragma once
#include <pix/pix.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pix_frame_t; /* forward */

/**
 * @ingroup pix
 * Decode a JPEG from a contiguous memory buffer into a newly allocated frame.
 * On success returns a frame with pixels allocated and frame->destroy set;
 * caller releases with: if(frame->destroy) frame->destroy(frame);
 * free(frame);
 */
struct pix_frame_t *pix_frame_init_jpeg(const void *data, size_t size,
                                        pix_format_t format);

/** User-supplied streaming read callback: return number of bytes read (0 =
 * EOF/error). */
/**
 * User-supplied streaming read callback.
 * @param data Destination buffer to fill.
 * @param size Maximum bytes to read into data.
 * @param user_data Opaque pointer provided at stream creation.
 * @return Number of bytes actually read (0 = EOF or error).
 */
/** @ingroup pix */
typedef size_t (*pix_jpeg_read_cb)(void *data, size_t size, void *user_data);

/**
 * @ingroup pix
 * Stream a JPEG from an abstract data source using read_cb. The decoder will
 * request bytes sequentially; the callback should return 0 on EOF or error.
 * The entire decompressed image is written directly into the destination
 * frame (no full-size intermediate RGB buffer). Release the returned frame
 * via frame->destroy then free(). Returns NULL on failure.
 */
struct pix_frame_t *pix_frame_init_jpeg_stream(pix_jpeg_read_cb read_cb,
                                               void *user_data,
                                               pix_format_t format);

#ifdef __cplusplus
}
#endif
