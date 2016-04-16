#include "DemuxingContext.h"


/* Enable or disable frame reference counting. You are not supposed to support
 * both paths in your application but pick the one most appropriate to your
 * needs. Look for the use of refcount in this example to see what are the
 * differences of API usage between them. */

static int open_codec_context(IOFileName &files, DemuxingVideoAudioContex &demux_ctx, enum AVMediaType type)
{
	int ret, stream_index;
	AVStream *st;
	AVCodecContext *dec_ctx = NULL;
	AVCodec *dec = NULL;
	AVDictionary *opts = NULL;

	ret = av_find_best_stream(demux_ctx.fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) 
	{
		fprintf(stderr, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(type), files.src_filename);
		return ret;
	} 
	else 
	{
		stream_index = ret;
		st = demux_ctx.fmt_ctx->streams[stream_index];

		/* find decoder for the stream */
		dec_ctx = st->codec;
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) 
		{
			fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Init the decoders, with or without reference counting */
		if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) 
		{
			fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
			return ret;
		}

		switch (type)
		{
		case AVMEDIA_TYPE_VIDEO:
			demux_ctx.video_stream_idx = stream_index;
			demux_ctx.video_stream = demux_ctx.fmt_ctx->streams[stream_index];
			demux_ctx.video_dec_ctx = demux_ctx.video_stream->codec;
			break;
		case AVMEDIA_TYPE_AUDIO:
			demux_ctx.audio_stream_idx = stream_index;
			demux_ctx.audio_stream = demux_ctx.fmt_ctx->streams[stream_index];
			demux_ctx.audio_dec_ctx = demux_ctx.audio_stream->codec;
			break;
		default:
			fprintf(stderr, "Error: unsupported MediaType: %s\n", av_get_media_type_string(type));
			return -1;
		}
	}

	return 0;
}

int InitDemuxContext(IOFileName &files, DemuxingVideoAudioContex &demux_ctx)
{
	int ret = 0, width, height;

	/* register all formats and codecs */
	av_register_all();

	/* open input file, and allocate format context */
	if (avformat_open_input(&(demux_ctx.fmt_ctx), files.src_filename, NULL, NULL) < 0)
	{
		fprintf(stderr, "Could not open source file %s\n", files.src_filename);
		return -1;
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(demux_ctx.fmt_ctx, NULL) < 0) 
	{
		fprintf(stderr, "Could not find stream information\n");
		return -1;
	}

	if (open_codec_context(files, demux_ctx, AVMEDIA_TYPE_VIDEO) >= 0) 
	{
// 		files.video_dst_file = fopen(files.dst_filename, "wb");
// 		if (!files.video_dst_file) 
// 		{
// 			fprintf(stderr, "Could not open destination file %s\n", files.dst_filename);
// 			return -1;
// 		}

		files.temp_file = fopen("temp.yuv", "wb");
		if (!files.temp_file) 
		{
			fprintf(stderr, "Could not open destination file %s\n", files.temp_file);
			return -1;
		}

		/* allocate image where the decoded image will be put */
		demux_ctx.width = demux_ctx.video_dec_ctx->width;
		demux_ctx.height = demux_ctx.video_dec_ctx->height;
		demux_ctx.pix_fmt = demux_ctx.video_dec_ctx->pix_fmt;
		ret = av_image_alloc(demux_ctx.video_dst_data, demux_ctx.video_dst_linesize, demux_ctx.width, demux_ctx.height, demux_ctx.pix_fmt, 1);
		if (ret < 0) 
		{
			fprintf(stderr, "Could not allocate raw video buffer\n");
			return -1;
		}
		demux_ctx.video_dst_bufsize = ret;
	}


	if (demux_ctx.video_stream)
	{
		printf("Demuxing video from file '%s' into '%s'\n", files.src_filename, files.dst_filename);
	}

	/* dump input information to stderr */
	av_dump_format(demux_ctx.fmt_ctx, 0, files.src_filename, 0);

	if (!demux_ctx.audio_stream && !demux_ctx.video_stream) 
	{
		fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
		return -1;
	}

	return 0;
}

void CloseDemuxContext(IOFileName &files, DemuxingVideoAudioContex &demux_ctx)
{
	avcodec_close(demux_ctx.video_dec_ctx);
	avcodec_close(demux_ctx.audio_dec_ctx);
	avformat_close_input(&(demux_ctx.fmt_ctx));
	av_frame_free(&demux_ctx.frame);
	av_free(demux_ctx.video_dst_data[0]);

	if (files.video_dst_file)
		fclose(files.video_dst_file);
	if (files.audio_dst_file)
		fclose(files.audio_dst_file);
}

