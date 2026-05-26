#include "AudioFilter.hpp"
#include <utility>

using namespace FFmpeg;

AudioFilter::AudioFilter (std::string filterSpec)
    : filterSpec (std::move (filterSpec))
{
}

AudioFilter::~AudioFilter ()
{
    if (graph != nullptr)
    {
        avfilter_graph_free (&graph);
    }
    source = nullptr;
    sink = nullptr;
}

void AudioFilter::init (const Frame &frame)
{
    const AVFilter *abuffer = avfilter_get_by_name ("abuffer");
    if (abuffer == nullptr)
    {
        throw std::runtime_error ("Could not find abuffer filter");
    }

    const AVFilter *abuffersink = avfilter_get_by_name ("abuffersink");
    if (abuffersink == nullptr)
    {
        throw std::runtime_error ("Could not find abuffersink filter");
    }

    AVFilterGraph *nextGraph = avfilter_graph_alloc ();
    if (nextGraph == nullptr)
    {
        throw std::runtime_error ("Could not allocate AVFilterGraph");
    }

    AVFilterContext *nextSource = nullptr;
    AVFilterContext *nextSink = nullptr;
    AVFilterInOut *inputs = nullptr;
    AVFilterInOut *outputs = nullptr;
    AVFilterInOut *rawInputs = nullptr;
    AVFilterInOut *rawOutputs = nullptr;
    AVBufferSrcParameters *sourceParameters = nullptr;

    try
    {
        nextSource = avfilter_graph_alloc_filter (nextGraph, abuffer, "in");
        if (nextSource == nullptr)
        {
            throw std::runtime_error ("Could not allocate abuffer filter");
        }

        sourceParameters = av_buffersrc_parameters_alloc ();
        if (sourceParameters == nullptr)
        {
            throw std::runtime_error (
                "Could not allocate AVBufferSrcParameters");
        }

        int ret = av_channel_layout_copy (&sourceParameters->ch_layout,
                                          &frame->ch_layout);
        checkError (ret, "av_channel_layout_copy");

        sourceParameters->format = frame->format;
        sourceParameters->time_base = frame->time_base;
        sourceParameters->sample_rate = frame->sample_rate;

        ret = av_buffersrc_parameters_set (nextSource, sourceParameters);
        checkError (ret, "av_buffersrc_parameters_set");
        av_channel_layout_uninit (&sourceParameters->ch_layout);
        av_freep (&sourceParameters);

        ret = avfilter_init_str (nextSource, nullptr);
        checkError (ret, "avfilter_init_str abuffer");

        ret = avfilter_graph_create_filter (&nextSink, abuffersink, "out",
                                            nullptr, nullptr, nextGraph);
        checkError (ret, "avfilter_graph_create_filter abuffersink");

        outputs = avfilter_inout_alloc ();
        inputs = avfilter_inout_alloc ();
        if (outputs == nullptr || inputs == nullptr)
        {
            throw std::runtime_error ("Could not allocate AVFilterInOut");
        }

        outputs->name = av_strdup ("in");
        outputs->filter_ctx = nextSource;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        inputs->name = av_strdup ("out");
        inputs->filter_ctx = nextSink;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        if (outputs->name == nullptr || inputs->name == nullptr)
        {
            throw std::runtime_error (
                "Could not allocate filter endpoint names");
        }

        rawInputs = inputs;
        rawOutputs = outputs;
        inputs = nullptr;
        outputs = nullptr;
        ret = avfilter_graph_parse_ptr (nextGraph, filterSpec.c_str (),
                                        &rawInputs, &rawOutputs, nullptr);
        avfilter_inout_free (&rawInputs);
        avfilter_inout_free (&rawOutputs);
        checkError (ret, "avfilter_graph_parse_ptr");

        ret = avfilter_graph_config (nextGraph, nullptr);
        checkError (ret, "avfilter_graph_config");

        graph = nextGraph;
        source = nextSource;
        sink = nextSink;
        nextGraph = nullptr;
    }
    catch (...)
    {
        avfilter_inout_free (&inputs);
        avfilter_inout_free (&outputs);
        avfilter_inout_free (&rawInputs);
        avfilter_inout_free (&rawOutputs);
        if (sourceParameters != nullptr)
        {
            av_channel_layout_uninit (&sourceParameters->ch_layout);
        }
        av_freep (&sourceParameters);
        avfilter_graph_free (&nextGraph);
        throw;
    }
}

auto AudioFilter::receive () -> std::vector<Frame>
{
    std::vector<Frame> frames;
    while (true)
    {
        Frame frame;
        int ret = av_buffersink_get_frame (sink, frame.get ());
        if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        checkError (ret, "av_buffersink_get_frame");
        frames.push_back (std::move (frame));
    }
    return frames;
}

auto AudioFilter::filter (const Frame &frame) -> std::vector<Frame>
{
    std::lock_guard lock (mutex);
    if (frame->width != 0 || frame->height != 0 || frame->nb_samples <= 0)
    {
        throw std::runtime_error ("AudioFilter only supports audio frames");
    }

    if (filterSpec.empty ())
    {
        return { frame };
    }

    if (graph == nullptr || source == nullptr || sink == nullptr)
    {
        init (frame);
    }

    int ret = av_buffersrc_add_frame_flags (
        source, const_cast<AVFrame *> (frame.get ()),
        AV_BUFFERSRC_FLAG_KEEP_REF);
    checkError (ret, "av_buffersrc_add_frame_flags");

    return receive ();
}

auto AudioFilter::flush () -> std::vector<Frame>
{
    std::lock_guard lock (mutex);
    if (graph == nullptr || source == nullptr || sink == nullptr)
    {
        return {};
    }

    int ret = av_buffersrc_add_frame_flags (source, nullptr, 0);
    checkError (ret, "av_buffersrc_add_frame_flags flush");
    return receive ();
}
