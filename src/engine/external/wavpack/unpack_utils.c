////////////////////////////////////////////////////////////////////////////
//                           **** WAVPACK ****                            //
//                  Hybrid Lossless Wavefile Compressor                   //
//              Copyright (c) 1998 - 2013 Conifer Software.               //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

// unpack_utils.c

// This module provides the high-level API for unpacking audio data from
// WavPack files. It manages the buffers used to interleave the data passed
// back to the application from the individual streams. The actual audio
// stream decompression is handled in the unpack.c module.

#include <stdlib.h>
#include <string.h>

#include "wavpack_local.h"

#ifdef ENABLE_THREADS
static void unpack_samples_enqueue (WavpackStream *wps, int32_t *outbuf, int offset, uint32_t samcnt, int free_wps);
static void worker_threads_finish (WavpackContext *wpc);
static void worker_threads_create (WavpackContext *wpc);
static int worker_available (WavpackContext *wpc);
#endif

///////////////////////////// executable code ////////////////////////////////

// This function unpacks the specified number of samples from the given stream (which must be
// completely loaded and initialized). The samples are written (interleaved) into the given
// buffer at the specified offset. This function is threadsafe across streams, so it may be
// called directly from the main unpack code or from the worker threads.

static void unpack_samples_interleave (WavpackStream *wps, int32_t *outbuf, int offset, int32_t *tmpbuf, uint32_t samcnt)
{
    int num_channels = wps->wpc->config.num_channels;
    int32_t *src = tmpbuf, *dst = outbuf + offset;

    // if the number of channels in the stream exactly matches the channels in the file, we
    // don't actually have to interleave and can render directly without the temp buffer

    if (num_channels == (wps->wphdr.flags & MONO_FLAG ? 1 : 2)) {
#ifdef ENABLE_DSD
        if (wps->wphdr.flags & DSD_FLAG)
            unpack_dsd_samples (wps, outbuf, samcnt);
        else
#endif
            unpack_samples (wps, outbuf, samcnt);

        return;
    }

    // otherwise we do have to use the temp buffer

#ifdef ENABLE_DSD
    if (wps->wphdr.flags & DSD_FLAG)
        unpack_dsd_samples (wps, tmpbuf, samcnt);
    else
#endif
        unpack_samples (wps, tmpbuf, samcnt);

    // if the block is mono, copy the samples from the single channel into the destination
    // using num_channels as the stride

    if (wps->wphdr.flags & MONO_FLAG) {
        while (samcnt--) {
            dst [0] = *src++;
            dst += num_channels;
        }
    }

    // if the block is stereo, and we don't have room for two more channels, just copy one

    else if (offset == num_channels - 1) {
        while (samcnt--) {
            dst [0] = src [0];
            dst += num_channels;
            src += 2;
        }
    }

    // otherwise copy the stereo samples into the destination

    else {
        while (samcnt--) {
            dst [0] = *src++;
            dst [1] = *src++;
            dst += num_channels;
        }
    }
}

// Unpack the specified number of samples from the current file position.
// Note that "samples" here refers to "complete" samples, which would be
// 2 longs for stereo files or even more for multichannel files, so the
// required memory at "buffer" is 4 * samples * num_channels bytes. The
// audio data is returned right-justified in 32-bit longs in the endian
// mode native to the executing processor. So, if the original data was
// 16-bit, then the values returned would be +/-32k. Floating point data
// can also be returned if the source was floating point data (and this
// can be optionally normalized to +/-1.0 by using the appropriate flag
// in the call to WavpackOpenFileInput ()). The actual number of samples
// unpacked is returned, which should be equal to the number requested unless
// the end of file is encountered or an error occurs. After all samples have
// been unpacked then 0 will be returned.

uint32_t WavpackUnpackSamples (WavpackContext *wpc, int32_t *buffer, uint32_t samples)
{
    int num_channels = wpc->config.num_channels, file_done = FALSE;
    uint32_t bcount, samples_unpacked = 0, samples_to_unpack;
    int32_t *bptr = buffer;

    memset (buffer, 0, (wpc->reduced_channels ? wpc->reduced_channels : num_channels) * samples * sizeof (int32_t));

#ifdef ENABLE_THREADS
    if (wpc->num_workers && !wpc->workers)
        worker_threads_create (wpc);

    wpc->worker_errors = 0;
#endif

#ifdef ENABLE_LEGACY
    if (wpc->stream3)
        return unpack_samples3 (wpc, buffer, samples);
#endif

    while (samples) {
        WavpackStream *wps = wpc->streams [0];
        int stream_index = 0;

        // if the current block has no audio, or it's not the first block of a multichannel
        // sequence, or the sample we're on is past the last sample in this block...we need
        // to free up the streams and read the next block

        if (!wps->wphdr.block_samples || !(wps->wphdr.flags & INITIAL_BLOCK) ||
            wps->sample_index >= GET_BLOCK_INDEX (wps->wphdr) + wps->wphdr.block_samples) {
                int64_t nexthdrpos;

                if (wpc->wrapper_bytes >= MAX_WRAPPER_BYTES)
                    break;

                free_streams (wpc);
                nexthdrpos = wpc->reader->get_pos (wpc->wv_in);
                bcount = read_next_header (wpc->reader, wpc->wv_in, &wps->wphdr);

                if (bcount == (uint32_t) -1)
                    break;

                wpc->filepos = nexthdrpos + bcount;

                // allocate the memory for the entire raw block and read it in

                wps->blockbuff = (unsigned char *)malloc (wps->wphdr.ckSize + 8);

                if (!wps->blockbuff)
                    break;

                memcpy (wps->blockbuff, &wps->wphdr, 32);

                if (wpc->reader->read_bytes (wpc->wv_in, wps->blockbuff + 32, wps->wphdr.ckSize - 24) !=
                    wps->wphdr.ckSize - 24) {
                        strcpy (wpc->error_message, "can't read all of last block!");
                        wps->wphdr.block_samples = 0;
                        wps->wphdr.ckSize = 24;
                        break;
                }

                // render corrupt blocks harmless
                if (!WavpackVerifySingleBlock (wps->blockbuff, !(wpc->open_flags & OPEN_NO_CHECKSUM))) {
                    wps->wphdr.ckSize = sizeof (WavpackHeader) - 8;
                    wps->wphdr.block_samples = 0;
                    memcpy (wps->blockbuff, &wps->wphdr, 32);
                }

                // potentially adjusting block_index must be done AFTER verifying block

                if (wpc->open_flags & OPEN_STREAMING)
                    SET_BLOCK_INDEX (wps->wphdr, wps->sample_index = 0);
                else
                    SET_BLOCK_INDEX (wps->wphdr, GET_BLOCK_INDEX (wps->wphdr) - wpc->initial_index);

                memcpy (wps->blockbuff, &wps->wphdr, 32);
                wps->init_done = FALSE;     // we have not yet called unpack_init() for this block

                // if this block has audio, but not the sample index we were expecting, flag an error

                if (!wpc->reduced_channels && wps->wphdr.block_samples && wps->sample_index != GET_BLOCK_INDEX (wps->wphdr))
                    wpc->crc_errors++;

                // if this block has audio, and we're in hybrid lossless mode, read the matching wvc block

                if (wps->wphdr.block_samples && wpc->wvc_flag)
                    read_wvc_block (wpc, 0);

                // if the block does NOT have any audio, call unpack_init() to process non-audio stuff

                if (!wps->wphdr.block_samples) {
                    if (!wps->init_done && !unpack_init (wpc, 0))
                        wpc->crc_errors++;

                    wps->init_done = TRUE;
                }
        }

        // if the current block has no audio, or it's not the first block of a multichannel
        // sequence, or the sample we're on is past the last sample in this block...we need
        // to loop back and read the next block

        if (!wps->wphdr.block_samples || !(wps->wphdr.flags & INITIAL_BLOCK) ||
            wps->sample_index >= GET_BLOCK_INDEX (wps->wphdr) + wps->wphdr.block_samples)
                continue;

        // There seems to be some missing data, like a block was corrupted or something.
        // If it's not too much data, just fill in with silence here and loop back.

        if (wps->sample_index < GET_BLOCK_INDEX (wps->wphdr)) {
            int32_t zvalue = (wps->wphdr.flags & DSD_FLAG) ? 0x55 : 0;

            samples_to_unpack = (uint32_t) (GET_BLOCK_INDEX (wps->wphdr) - wps->sample_index);

            if (!samples_to_unpack || samples_to_unpack > 262144) {
                strcpy (wpc->error_message, "discontinuity found, aborting file!");
                wps->wphdr.block_samples = 0;
                wps->wphdr.ckSize = 24;
                break;
            }

            if (samples_to_unpack > samples)
                samples_to_unpack = samples;

            wps->sample_index += samples_to_unpack;
            samples_unpacked += samples_to_unpack;
            samples -= samples_to_unpack;

            samples_to_unpack *= (wpc->reduced_channels ? wpc->reduced_channels : num_channels);

            while (samples_to_unpack--)
                *bptr++ = zvalue;

            continue;
        }

        // calculate number of samples to process from this block, then initialize the decoder for
        // this block if we haven't already

        samples_to_unpack = (uint32_t) (GET_BLOCK_INDEX (wps->wphdr) + wps->wphdr.block_samples - wps->sample_index);

        if (samples_to_unpack > samples)
            samples_to_unpack = samples;

        if (!wps->init_done && !unpack_init (wpc, 0))
            wpc->crc_errors++;

        wps->init_done = TRUE;

        // if this block is not the final block of a multichannel sequence (and we're not truncating
        // to stereo), then enter this conditional block...otherwise we just unpack the samples directly

        if (!wpc->reduced_channels && !(wps->wphdr.flags & FINAL_BLOCK)) {
            int32_t *temp_buffer = (int32_t *)calloc (1, samples_to_unpack * 8);
            uint32_t offset = 0;     // offset to next channel in sequence (0 to num_channels - 1)

            // loop through all the streams...

            while (1) {

                // if the stream has not been allocated and corresponding block read, do that here...

                if (stream_index == wpc->num_streams) {
                    wpc->streams = (WavpackStream **)realloc (wpc->streams, (wpc->num_streams + 1) * sizeof (wpc->streams [0]));

                    if (!wpc->streams)
                        break;

                    wps = wpc->streams [wpc->num_streams++] = (WavpackStream *)calloc (1, sizeof (WavpackStream));

                    if (!wps)
                        break;

                    wps->wpc = wpc;
                    wps->stream_index = stream_index;
                    bcount = read_next_header (wpc->reader, wpc->wv_in, &wps->wphdr);

                    if (bcount == (uint32_t) -1) {
                        wpc->streams [0]->wphdr.block_samples = 0;
                        wpc->streams [0]->wphdr.ckSize = 24;
                        file_done = TRUE;
                        break;
                    }

                    wps->blockbuff = (unsigned char *)malloc (wps->wphdr.ckSize + 8);

                    if (!wps->blockbuff)
                        break;

                    memcpy (wps->blockbuff, &wps->wphdr, 32);

                    if (wpc->reader->read_bytes (wpc->wv_in, wps->blockbuff + 32, wps->wphdr.ckSize - 24) !=
                        wps->wphdr.ckSize - 24) {
                            wpc->streams [0]->wphdr.block_samples = 0;
                            wpc->streams [0]->wphdr.ckSize = 24;
                            file_done = TRUE;
                            break;
                    }

                    // render corrupt blocks harmless
                    if (!WavpackVerifySingleBlock (wps->blockbuff, !(wpc->open_flags & OPEN_NO_CHECKSUM))) {
                        wps->wphdr.ckSize = sizeof (WavpackHeader) - 8;
                        wps->wphdr.block_samples = 0;
                        memcpy (wps->blockbuff, &wps->wphdr, 32);
                    }

                    // potentially adjusting block_index must be done AFTER verifying block

                    if (wpc->open_flags & OPEN_STREAMING)
                        SET_BLOCK_INDEX (wps->wphdr, wps->sample_index = 0);
                    else
                        SET_BLOCK_INDEX (wps->wphdr, GET_BLOCK_INDEX (wps->wphdr) - wpc->initial_index);

                    memcpy (wps->blockbuff, &wps->wphdr, 32);

                    // if this block has audio, and we're in hybrid lossless mode, read the matching wvc block

                    if (wpc->wvc_flag)
                        read_wvc_block (wpc, stream_index);

                    // initialize the unpacker for this block

                    if (!unpack_init (wpc, stream_index))
                        wpc->crc_errors++;

                    wps->init_done = TRUE;
                }
                else
                    wps = wpc->streams [stream_index];

#ifdef ENABLE_THREADS
                // If there is a worker thread available, and there have been no errors, and this is not
                // the final block of the multichannel stream, then give it to a worker thread to decode.
                // To reduce context switches, we do the final block in the foreground because we have to
                // wait around anyway for all the workers to complete.

                if (worker_available (wpc) && !wps->mute_error && !(wps->wphdr.flags & FINAL_BLOCK))
                    unpack_samples_enqueue (wps, bptr, offset, samples_to_unpack, FALSE);
                else
#endif
                {
                    unpack_samples_interleave (wps, bptr, offset, temp_buffer, samples_to_unpack);

                    if (wps->sample_index == GET_BLOCK_INDEX (wps->wphdr) + wps->wphdr.block_samples && wps->mute_error)
                        wpc->crc_errors++;
                }

                if (wps->wphdr.flags & MONO_FLAG)
                    offset++;
                else if (offset == num_channels - 1) {
                    wpc->crc_errors++;
                    offset++;
                }
                else
                    offset += 2;

                // check several clues that we're done with this set of blocks and exit if we are; else do next stream

                if ((wps->wphdr.flags & FINAL_BLOCK) || stream_index == wpc->max_streams - 1 || offset == num_channels)
                    break;
                else
                    stream_index++;
            }

#ifdef ENABLE_THREADS
            worker_threads_finish (wpc);    // for multichannel, wait for all threads to finish before freeing anything
#endif

            free (temp_buffer);

            // if we didn't get all the channels we expected, mute the buffer and flag an error

            if (offset != num_channels) {
                if (wps->wphdr.flags & DSD_FLAG) {
                    int samples_to_zero = samples_to_unpack * num_channels;
                    int32_t *zptr = bptr;

                    while (samples_to_zero--)
                        *zptr++ = 0x55;
                }
                else
                    memset (bptr, 0, samples_to_unpack * num_channels * 4);

                wpc->crc_errors++;
            }

            // go back to the first stream (we're going to leave them all loaded for now because they might have more samples)

            wps = wpc->streams [stream_index = 0];
        }
        // catch the error situation where we have only one channel but run into a stereo block
        // (this avoids overwriting the caller's buffer)
        else if (!(wps->wphdr.flags & MONO_FLAG) && (num_channels == 1 || wpc->reduced_channels == 1)) {
            memset (bptr, 0, samples_to_unpack * sizeof (*bptr));
            wps->sample_index += samples_to_unpack;
            wpc->crc_errors++;
        }
#ifdef ENABLE_THREADS
        // This is where temporal multithreaded decoding occurs. If there's a worker thread available, and no errors
        // have been encountered, and we are going to finish this block, and there are samples beyond this block to
        // be decoded during this same call to WavpackUnpackSamples(), then we give this block to a worker thread to
        // decode to completion. Because we are going to continue decoding the next block in this stream before this
        // one completes, we must make a copy of the stream that can decode in isolation, and we instruct the worker
        // thread to free everything associated with the stream context when it's done.
        else if (worker_available (wpc) && !wps->mute_error &&
            wps->sample_index + samples_to_unpack == GET_BLOCK_INDEX (wps->wphdr) + wps->wphdr.block_samples &&
            wps->sample_index + samples > GET_BLOCK_INDEX (wps->wphdr) + wps->wphdr.block_samples) {
                WavpackStream *wps_copy = malloc (sizeof (WavpackStream));

                memcpy (wps_copy, wps, sizeof (WavpackStream));

                // Update the existing WavpackStream so we can use it for the next block before the current one
                // is complete. Because the worker thread will free any allocated areas, we mark those NULL here.
                // Also advance the sample index (even though they're really not decoded yet).

                wps->blockbuff = NULL;
                wps->block2buff = NULL;
                wps->sample_index += samples_to_unpack;

#ifdef ENABLE_DSD
                wps->dsd.probabilities = NULL;
                wps->dsd.summed_probabilities = NULL;
                wps->dsd.lookup_buffer = NULL;
                wps->dsd.value_lookup = NULL;
                wps->dsd.ptable = NULL;
#endif

                unpack_samples_enqueue (wps_copy, bptr, 0, samples_to_unpack, TRUE);
        }
#endif
        else {
#ifdef ENABLE_DSD
            if (wps->wphdr.flags & DSD_FLAG)
                unpack_dsd_samples (wps, bptr, samples_to_unpack);
            else
#endif
                unpack_samples (wps, bptr, samples_to_unpack);

            if (wps->sample_index == GET_BLOCK_INDEX (wps->wphdr) + wps->wphdr.block_samples && wps->mute_error)
                wpc->crc_errors++;
        }

        if (file_done) {
            strcpy (wpc->error_message, "can't read all of last block!");
            break;
        }

        if (wpc->reduced_channels)
            bptr += samples_to_unpack * wpc->reduced_channels;
        else
            bptr += samples_to_unpack * num_channels;

        samples_unpacked += samples_to_unpack;
        samples -= samples_to_unpack;

        if (wpc->total_samples != -1 && wps->sample_index == wpc->total_samples)
            break;
    }

#ifdef ENABLE_THREADS
    worker_threads_finish (wpc);    // we don't return until all decoding by worker threads is complete
#endif

#ifdef ENABLE_DSD
    if (wpc->decimation_context)    // TODO: this could be parallelized too
        decimate_dsd_run (wpc->decimation_context, buffer, samples_unpacked);
#endif

    return samples_unpacked;
}

///////////////////////////// multithreading code ////////////////////////////////

#ifdef ENABLE_THREADS

// This is the worker thread function for unpacking support, essentially allowing
// unpack_samples_interleave() to be running for multiple streams simultaneously.

#ifdef _WIN32
static unsigned WINAPI unpack_samples_worker_thread (LPVOID param)
#else
static void *unpack_samples_worker_thread (void *param)
#endif
{
    WorkerInfo *cxt = param;
    int32_t *temp_buffer = NULL;
    uint32_t temp_samples = 0;

    while (1) {
        wp_mutex_obtain (*cxt->mutex);
        cxt->state = Ready;
        (*cxt->workers_ready)++;
        wp_condvar_signal (*cxt->global_cond);      // signal that we're ready to work

        while (cxt->state == Ready)                 // wait for something to do
            wp_condvar_wait (cxt->worker_cond, *cxt->mutex);

        wp_mutex_release (*cxt->mutex);

        if (cxt->state == Quit)                     // break out if we're done
            break;

        if (cxt->samcnt > temp_samples) {           // reallocate temp buffer if not big enough
            temp_buffer = (int32_t *) realloc (temp_buffer, (temp_samples = cxt->samcnt) * 8);
            memset (temp_buffer, 0, temp_samples * 8);
        }

        // this is where the work is done
        unpack_samples_interleave (cxt->wps, cxt->outbuf, cxt->offset, temp_buffer, cxt->samcnt);

        if (cxt->wps->mute_error) {                 // this is where we pass back decoding errors
            wp_mutex_obtain (*cxt->mutex);
            (*cxt->worker_errors)++;
            wp_mutex_release (*cxt->mutex);
        }

        if (cxt->free_wps) {                        // if instructed, free the WavpackStream context
            free_single_stream (cxt->wps);
            free (cxt->wps);
        }
    }

    free (temp_buffer);
    wp_thread_exit (0);
    return 0;
}

// Send the given stream to an available worker thread. In the background, the stream will be
// unpacked and written (interleaved) to the given buffer at the specified offset. The "free_wps"
// flag indicates that the WavpackStream structure should be freed once the unpack operation is
// complete because it is a copy of the original created for this operation only.

static void unpack_samples_enqueue (WavpackStream *wps, int32_t *outbuf, int offset, uint32_t samcnt, int free_wps)
{
    WavpackContext *wpc = (WavpackContext *) wps->wpc;  // this is safe here because single-threaded
    int i;

    wp_mutex_obtain (wpc->mutex);

    while (!wpc->workers_ready)
        wp_condvar_wait (wpc->global_cond, wpc->mutex);

    for (i = 0; i < wpc->num_workers; ++i)
        if (wpc->workers [i].state == Ready) {
            wpc->workers [i].wps = wps;
            wpc->workers [i].state = Running;
            wpc->workers [i].outbuf = outbuf;
            wpc->workers [i].offset = offset;
            wpc->workers [i].samcnt = samcnt;
            wpc->workers [i].free_wps = free_wps;
            wp_condvar_signal (wpc->workers [i].worker_cond);
            wpc->workers_ready--;
            break;
        }

    wp_mutex_release (wpc->mutex);
}

static void worker_threads_finish (WavpackContext *wpc)
{
    if (wpc->workers) {
        wp_mutex_obtain (wpc->mutex);

        while (wpc->workers_ready < wpc->num_workers)
            wp_condvar_wait (wpc->global_cond, wpc->mutex);

        wp_mutex_release (wpc->mutex);
    }

    if (wpc->worker_errors) {
        wpc->crc_errors += wpc->worker_errors;
        wpc->worker_errors = 0;
    }
}

// Create the worker thread contexts and start the threads
// (which should all quickly go to the ready state)

static void worker_threads_create (WavpackContext *wpc)
{
    if (!wpc->workers) {
        int i;

        wp_mutex_init (wpc->mutex);
        wp_condvar_init (wpc->global_cond);

        wpc->workers = calloc (wpc->num_workers, sizeof (WorkerInfo));

        for (i = 0; i < wpc->num_workers; ++i) {
            wpc->workers [i].mutex = &wpc->mutex;
            wpc->workers [i].global_cond = &wpc->global_cond;
            wpc->workers [i].workers_ready = &wpc->workers_ready;
            wpc->workers [i].worker_errors = &wpc->worker_errors;
            wp_condvar_init (wpc->workers [i].worker_cond);
            wp_thread_create (wpc->workers [i].thread, unpack_samples_worker_thread, &wpc->workers [i]);

            // gracefully handle failures in creating worker threads

            if (!wpc->workers [i].thread) {
                wp_condvar_delete (wpc->workers [i].worker_cond);
                wpc->num_workers = i;
                break;
            }
        }

        if (!wpc->num_workers) {    // if we failed to start any workers, free the array
            free (wpc->workers);
            wpc->workers = NULL;
        }
    }
}

// Return TRUE if there is a worker thread available. Obviously this depends on
// whether there are worker threads enabled and running, but it also depends
// on whether there is actually a worker thread that's not busy (however we
// don't count this as a requirement if there are many workers).

static int worker_available (WavpackContext *wpc)
{
    int retval = FALSE;

    if (wpc->num_workers && wpc->workers) {
        if (wpc->num_workers < 4) {
            wp_mutex_obtain (wpc->mutex);
            retval = wpc->workers_ready;
            wp_mutex_release (wpc->mutex);
        }
        else
            retval = TRUE;
    }

    return retval;
}
#endif
