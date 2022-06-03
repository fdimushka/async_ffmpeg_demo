#include <iostream>
#include <ar/ar.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
}

using namespace AsyncRuntime;



typedef struct io_network_context {
    int fd;
    TCPConnectionPtr connection;
    IOStreamPtr read_stream;
    IOStreamPtr write_stream;
    int read_seek = 0;
    CoroutineHandler* handler;
} io_network_context;



static int recv_cb(void *buf, size_t buf_size, int flags, void *opaque) {
    auto *ctx = (io_network_context*)opaque;

    if(!ctx->read_stream || ctx->read_stream->GetBufferSize() - ctx->read_seek <= 0) {
        ctx->read_stream = MakeStream();
        ctx->read_seek = 0;

        int ret = Await(AsyncRead(ctx->connection, ctx->read_stream), ctx->handler);
        if (ret != IO_SUCCESS || ctx->read_stream->GetBufferSize() <= 0) {
            std::cerr << "error: " << FSErrorMsg(ret) << std::endl;
            return 0;
        }

        buf_size = std::min(buf_size, (size_t)(ctx->read_stream->GetBufferSize() - ctx->read_seek));
        memcpy(buf, ctx->read_stream->GetBuffer() + ctx->read_seek, buf_size);
        ctx->read_seek += buf_size;
        return buf_size;
    } else if(ctx->read_stream->GetBufferSize() - ctx->read_seek > buf_size) {
        memcpy(buf, ctx->read_stream->GetBuffer() + ctx->read_seek, buf_size);
        ctx->read_seek += buf_size;
        return buf_size;
    }else if(ctx->read_stream->GetBufferSize() - ctx->read_seek <= buf_size) {
        buf_size = ctx->read_stream->GetBufferSize() - ctx->read_seek;
        memcpy(buf, ctx->read_stream->GetBuffer() + ctx->read_seek, buf_size);
        ctx->read_seek = 0;
        ctx->read_stream->Flush();

        return buf_size;
    }

    return 0;
}


static int send_cb(const void *buf, size_t buf_size, int flags, void *opaque) {
    auto *ctx = (io_network_context*)opaque;


    ctx->write_stream = MakeStream((const char*)buf, buf_size);
    int ret = Await(AsyncWrite(ctx->connection, ctx->write_stream), ctx->handler);
    if(ret == IO_SUCCESS) {
        ctx->write_stream->Flush();
        return buf_size;
    }else{
        std::cerr << "error: " << FSErrorMsg(ret) << std::endl;
        ctx->write_stream->Flush();
        return 0;
    }
}


static void close_cb(void *opaque) {
    auto *ctx = (io_network_context*)opaque;
    close(ctx->fd);
}


static void open_cb(int fd, AVIONetAdapter *adapter, void *opaque) {
    auto *ctx = (io_network_context*)opaque;
    ctx->fd = fd;

    ctx->connection = MakeTCPConnection(ctx->fd);
    int ret = Await(AsyncConnect(ctx->connection), ctx->handler);


    if(ret != IO_SUCCESS){
        std::cerr << "error: " << FSErrorMsg(ret) << std::endl;
    }

    avio_init_net_adapter(adapter, &recv_cb, &send_cb, &close_cb);
}


void async_process_stream(CoroutineHandler* handler, Yield<int>& yield, int id, const std::string& url)
{
    io_network_context io_ctx;
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket *pkt = NULL;
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);

    int ret = 0;

    if (!(ifmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ifmt_ctx->open_callback.callback = &open_cb;
    ifmt_ctx->open_callback.opaque = &io_ctx;
    io_ctx.handler = handler;
    yield(0);

    if ((ret = avformat_open_input(&ifmt_ctx, url.c_str(), 0, &opts)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", url.c_str());

        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, url.c_str(), 0);

    for(;;) {
        pkt = av_packet_alloc();

        if (!pkt) {
            fprintf(stderr, "Could not allocate AVPacket\n");
            yield(1);
        }

        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0) {
            goto end;
        }

        if(pkt->stream_index == 0) {
            std::cout << id << " " << pkt->pts << std::endl;
        }
        av_packet_free(&pkt);
    }

    end:
    avformat_close_input(&ifmt_ctx);

    if (ret < 0 && ret != AVERROR_EOF) {
        //fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        yield(1);
    }
}


int main() {
    SetupRuntime();
    auto stream_coro = MakeCoroutine<int>(&async_process_stream, 0, "rtsp link");
    Await(Async(stream_coro));
    Terminate();
    return 0;
}
