#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>

//#define SAVE_FILE
#define SHOW_YUV

void main(int argc, char* argv[])
{
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;

    AVFrame *pFrame;
    AVPacket *pPacket;

    FILE* f_raw = NULL;
    struct SwsContext *SwsCtx = NULL;
    SDL_Window* s_win = NULL;
    SDL_Renderer* s_ren = NULL;
    SDL_Texture* s_text = NULL;

    char* p_yuv = NULL;

    int ret_val, i, video_index = -1, audio_index = -1, r_nums = 0, w_nums = 0;
    float fps;

    av_register_all();

    if(argc != 2)
    {
        printf("You need specify a file!\n");
        return;
    }
    else
    {
        printf("Ready to play %s file!\n", argv[1]);
    }

#ifdef SAVE_FILE    
    f_raw = fopen("raw.yuv", "wb");
    if(f_raw == NULL)
    {
        printf("Create raw file false!\n");
        return;
    }
#endif

    ret_val = avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);
    if(ret_val != 0)
    {
        printf("Open input file error!\n");
        return;
    }

    av_dump_format(pFormatCtx, 0, argv[1], 0);

    avformat_find_stream_info(pFormatCtx, NULL);

    av_dump_format(pFormatCtx, 0, argv[1], 0);

    for(i=0; i<pFormatCtx->nb_streams; i++)
    {
        switch(pFormatCtx->streams[i]->codecpar->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
                fps = (float)pFormatCtx->streams[i]->r_frame_rate.num/(float)pFormatCtx->streams[i]->r_frame_rate.den;
                printf("Video index: %d, fps: %f\n", pFormatCtx->streams[i]->index, fps);
                video_index = pFormatCtx->streams[i]->index;
                break;

            case AVMEDIA_TYPE_AUDIO:
                printf("Audio index: %d\n", pFormatCtx->streams[i]->index);
                break;

            case AVMEDIA_TYPE_SUBTITLE:
                printf("Subtitle index: %d\n", pFormatCtx->streams[i]->index);
                break;

            default:
                break;
        }
    }

#ifdef SHOW_YUV
    if(SDL_Init(SDL_INIT_EVERYTHING))
    {
        printf("SDL initialize false! %s\n", SDL_GetError());
    }

    s_win = SDL_CreateWindow("Decoder", 100, 100, pFormatCtx->streams[video_index]->codecpar->width, pFormatCtx->streams[video_index]->codecpar->height, SDL_WINDOW_RESIZABLE);
    if(s_win == NULL)
    {
        printf("Create SDL windows false! %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    s_ren = SDL_CreateRenderer(s_win, -1, 0);
    if(s_ren == NULL)
    {
        printf("Create SDL renderer false! %s\n", SDL_GetError());
        SDL_DestroyWindow(s_win);
        SDL_Quit();
        return;
    }

    s_text = SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_IYUV,
                    SDL_TEXTUREACCESS_STREAMING, pFormatCtx->streams[video_index]->codecpar->width, pFormatCtx->streams[video_index]->codecpar->height);

#endif

    //Alloc codec context
    pCodecCtx = avcodec_alloc_context3(NULL);
    if(pCodecCtx == NULL)
    {
        printf("AVERROR(ENOMEM)");
        avformat_close_input(&pFormatCtx);
        return;
    }

    pPacket = av_packet_alloc();
    pFrame = av_frame_alloc();

    ret_val = avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[video_index]->codecpar);
    if(ret_val < 0)
    {
        goto error;
    }

#ifdef SHOW_YUV
    p_yuv = (char*)malloc(pFormatCtx->streams[video_index]->codecpar->width * pFormatCtx->streams[video_index]->codecpar->height * 2);
    if(p_yuv == NULL)
    {
        printf("malloc yuv data!\n");
        goto error;
    }
#endif

    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec == NULL)
    {
        printf("Could not find codec!\n");
        goto error;
    }
    else
    {
        printf("Found codec %s, fix_format: %d\n", pCodec->name, pFormatCtx->streams[video_index]->codecpar->format);
    }

    //open decoder
    ret_val = avcodec_open2(pCodecCtx, pCodec, NULL);
    if(ret_val < 0)
    {
        printf("Open decoder error %s!\n", strerror(-ret_val));
        goto error;
    }
    else
    {
        printf("Open decoder successful!\n");
    }

    //mainly deocder process
    while(1)
    {
        //read packet from fromat context
        ret_val = av_read_frame(pFormatCtx, pPacket);
        if(ret_val == AVERROR_EOF)
        {
            printf("EOF!\n");
            goto error;
        }
        r_nums++;

        //send packet to decoder
        ret_val = avcodec_send_packet(pCodecCtx, pPacket);
        av_packet_unref(pPacket);
        if(ret_val < 0)
        {
            printf("Send packet to decoder error %s!\n", strerror(-ret_val));
            goto error;
        }

        //got frame from decoder
        while(ret_val >= 0)
        {
            ret_val = avcodec_receive_frame(pCodecCtx, pFrame);
            if((ret_val == -EAGAIN) || (ret_val == AVERROR_EOF))
            {
                //printf(" %s\n", strerror(-ret_val));
                break;
            }

#ifdef SAVE_FILE
            //write YUV data to RAW file
            fwrite(pFrame->data[0], 1, pFrame->linesize[0] * pFrame->height, f_raw);
            fwrite(pFrame->data[1], 1, pFrame->linesize[1]/2 * pFrame->height, f_raw);
            fwrite(pFrame->data[2], 1, pFrame->linesize[2]/2 * pFrame->height, f_raw);
#endif
#ifdef SHOW_YUV
            memcpy(p_yuv, pFrame->data[0], pFrame->linesize[0] * pFrame->height);
            memcpy(p_yuv + pFrame->linesize[0] * pFrame->height, pFrame->data[1], pFrame->linesize[1] * pFrame->height/2);
            memcpy(p_yuv + (pFrame->linesize[0] + pFrame->linesize[1]/2) * pFrame->height, pFrame->data[2], pFrame->linesize[2] * pFrame->height/2);
            SDL_UpdateTexture(s_text, NULL, p_yuv, pFrame->linesize[0]);
            SDL_RenderClear(s_ren);
            SDL_RenderCopy(s_ren, s_text, NULL, NULL);
            SDL_RenderPresent(s_ren);
            SDL_Delay(1000/fps);
#endif
            w_nums++;
            av_frame_unref(pFrame);
        }
    }

error:
    av_frame_free(&pFrame);
    av_packet_free(&pPacket);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
#ifdef SAVE_FILE
    if(f_raw != NULL)
    {
        fclose(f_raw);
    }
#endif
#ifdef SHOW_YUV
    SDL_DestroyTexture(s_text);
    SDL_DestroyRenderer(s_ren);
    SDL_DestroyWindow(s_win);
    SDL_Quit();
    if(p_yuv)
    {
        free(p_yuv);
    }
#endif

    printf("Play END, Read %d packets, write %d frames!\n", r_nums, w_nums);
}
