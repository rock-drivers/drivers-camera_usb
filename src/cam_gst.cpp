#include "cam_gst.h"
#include <sys/time.h>
#include <boost/lexical_cast.hpp>

using namespace base::samples::frame;

namespace camera 
{


CamGst::InitGuard::InitGuard() {
            LOG_INFO("Initializing GStreamer");
            gst_init(NULL, NULL);
}

CamGst::InitGuard::~InitGuard() {
    LOG_INFO("Deinitializing GStreamer");
    gst_deinit();
}

// PUBLIC
CamGst::CamGst(std::string const& device) : mDevice(device), 
        mJpegQuality(DEFAULT_JPEG_QUALITY), 
        mLoop(NULL),
        mMainLoopThread(NULL),
        mPipeline(NULL),
        mGstPipelineBus(NULL),
        mPipelineRunning(false),
        mBuffer(NULL),
        mBufferSize(0),
        mNewBuffer(false),
        mSource(NULL),
        mFileDescriptor(-1),
        mRequestedFrameMode(MODE_UNDEFINED)
{
    LOG_DEBUG("CamGst: constructor");
    // gst_is_initialized() not available (since 0.10.31), 
    // could lead to a gst-mini-unref-warning.
    static InitGuard gInit;

    mLoop = g_main_loop_new (NULL, FALSE);
    pthread_mutex_init(&mMutexBuffer, NULL);
    LOG_DEBUG("Starting gst main loop thread");
    mMainLoopThread = new pthread_t();
    pthread_create(mMainLoopThread, NULL, mainLoop, (void*)mLoop);
}

CamGst::~CamGst() {
    LOG_DEBUG("CamGst: destructor");
    if(mBuffer != NULL) {
        gst_buffer_unref(mBuffer);
        mBuffer = NULL;
        mBufferSize = 0; 
    }
    deletePipeline();
    g_main_loop_quit(mLoop);
    g_main_loop_unref(mLoop);
    mLoop = NULL;
    pthread_mutex_destroy(&mMutexBuffer);
    pthread_join(*mMainLoopThread, NULL);
    delete mMainLoopThread;
    mMainLoopThread = NULL;
}

void CamGst::printElementFactories() {

    g_print("ELEMENT FACTORIES BEGIN ###################################################################\n");
    GList* elements = gst_registry_get_feature_list (gst_registry_get_default(), GST_TYPE_ELEMENT_FACTORY);
    for(;elements != NULL; elements = elements->next)
    {
        GstElementFactory* factory = GST_ELEMENT_FACTORY(elements->data);
        g_print ("The '%s' element is a member of the category %s.\n"
           "Description: %s\n",
           gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory)),
           gst_element_factory_get_klass (factory),
           gst_element_factory_get_description (factory));

    }
    g_print("ELEMENT FACTORIES END ###################################################################\n");
}

void CamGst::createDefaultPipeline(bool check_for_valid_params, 
        uint32_t width, uint32_t height, uint32_t fps, 
        uint32_t bpp, frame_mode_t image_mode,
        uint32_t jpeg_quality) {
    LOG_DEBUG("CamGst: createDefaultPipeline");
    deletePipeline();

    // Sets the passed parameters if possible, otherwise valid ones.
    if(check_for_valid_params) {
        setCameraParameters(&width, &height, &fps);
    }
    mJpegQuality = jpeg_quality;

    GstElement* source = createDefaultSource(mDevice);
    GstElement* colorspace  = gst_element_factory_make("ffmpegcolorspace", "colorspace");
    if(colorspace == NULL) {
        deletePipeline();
        throw CamGstException("Colorspace convertion element could not be created");
    }
    
    GstElement* sink = createDefaultSink();
    mSource = source;

    if((mPipeline = gst_pipeline_new ("default_pipeline")) == NULL) {
        deletePipeline();
        throw CamGstException("Default pipeline could not be created.");    
    }

    // Add a message handler.
    if(mGstPipelineBus != NULL) {
        gst_object_unref (mGstPipelineBus); 
        mGstPipelineBus = NULL;
    }
    mGstPipelineBus = gst_pipeline_get_bus (GST_PIPELINE (mPipeline));
    gst_bus_add_watch (mGstPipelineBus, callbackMessagesStatic, this);  

    GstElement* cap = 0;
   
    cap = createDefaultCap(width, height, fps, bpp, image_mode); // format
    if (image_mode == MODE_JPEG)
    {
        gst_bin_add_many (GST_BIN (mPipeline), source,  cap,  sink, (void*)NULL);
        if (!gst_element_link_many (source,  cap, sink, (void*)NULL)) {
            deletePipeline();
            throw CamGstException("Failed to link jpeg pipeline, try another image mode");
        }
    }
    else
    {
        gst_bin_add_many (GST_BIN (mPipeline), source, colorspace, cap, sink, (void*)NULL);
        if (!gst_element_link_many (source, colorspace, cap, sink, (void*)NULL)) {
            deletePipeline();
            throw CamGstException("Failed to link default pipeline, try another image mode");
        }
    }
    
    // Required to check if the image is a JPEG within getBuffer() (for header adaptions).
    mRequestedFrameMode = image_mode;
}

void CamGst::deletePipeline() {
    LOG_DEBUG("CamGst: deletePipeline");
    if(mPipeline == NULL) {
        LOG_INFO("Pipeline already deleted, return");
        return;
    }
    
    stopPipeline();

    gst_object_unref(GST_OBJECT(mGstPipelineBus)); 
    mGstPipelineBus = NULL;

    gst_object_unref(GST_OBJECT(mPipeline));
    mPipeline = NULL;
    mPipelineRunning = false;

    mNewBuffer = false;
}

// Print GstMessage
bool CamGst::startPipeline() {
    LOG_DEBUG("CamGst: startPipeline");
    if(mPipeline == NULL) {
        LOG_INFO("No pipeline available, can not be started");
        return false;
    }

    if(mPipelineRunning) {
        LOG_INFO("Pipeline already running, return true");
        return true;
    }

    GstState state;
    GstStateChangeReturn ret_state;

    ret_state = gst_element_set_state(mPipeline, GST_STATE_PLAYING);
    LOG_DEBUG("Set pipeline to playing returned %d",ret_state); 

    timeval start, end;
    gettimeofday(&start, 0);
    int time_passed_msec = 0;
    
    if(ret_state == GST_STATE_CHANGE_ASYNC) {
        // gst_element_get_state will return immediately (other than written in the gst-docu!)
        do {
            ret_state = gst_element_get_state(mPipeline, &state, NULL, DEFAULT_PIPELINE_TIMEOUT);
            gettimeofday(&end, 0);
            
            time_passed_msec = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
            if(time_passed_msec > (int)DEFAULT_PIPELINE_TIMEOUT) {
                LOG_ERROR("Pipeline could not be started. If you wanted to restart the pipeline, try to delete and recreate the pipeline instead");
                return false;
            }
        } while(ret_state == GST_STATE_CHANGE_ASYNC);
    } else {
        ret_state = gst_element_get_state(mPipeline, &state, NULL, DEFAULT_PIPELINE_TIMEOUT);
    }

    mPipelineRunning = (ret_state == GST_STATE_CHANGE_SUCCESS ? true : false);

    if(!mPipelineRunning) {
        GstMessage *msg;
        msg = gst_bus_poll (mGstPipelineBus, GST_MESSAGE_ERROR, 0);
        if (msg) {
            GError *err = NULL;
            gst_message_parse_error (msg, &err, NULL);
            LOG_ERROR("Pipeline could not be started: %s", err->message);
            g_error_free (err);
            gst_message_unref (msg);
        }
    }

    // Tries to set the file descriptor as well.
    readFileDescriptor();

    return mPipelineRunning;
}

void CamGst::stopPipeline() {
    LOG_DEBUG("CamGst: stopPipeline");
    if(!mPipelineRunning) {
        LOG_INFO("Pipeline already stopped");
        return;
    }

    // Setting to GST_STATE_NULL does not happen asynchronously, wait until stop.
    gst_element_set_state(mPipeline, GST_STATE_NULL);
    mPipelineRunning = false;

    rmFileDescriptor();
}

bool CamGst::getBuffer(std::vector<uint8_t>& buffer, bool blocking_read, 
        int32_t timeout) {
    LOG_DEBUG("CamGst: getBuffer");
    struct timeval start, end;
    long mtime=0, seconds=0, useconds=0; 
    if(timeout > 0) {
        gettimeofday(&start, NULL);
    }
    do { 
        pthread_mutex_lock(&mMutexBuffer);
        if(mBuffer == NULL || mBufferSize == 0 || mNewBuffer == false) {
            if(!blocking_read) { // !blocking: unlock and return.
                pthread_mutex_unlock(&mMutexBuffer);
                LOG_DEBUG("No image available");
                return false;
            } else {
                pthread_mutex_unlock(&mMutexBuffer);
                usleep(50); // blocking: wait
            }
        } else {
            // Copy buffer for return.
            buffer.resize(mBufferSize);
            memcpy(&buffer[0], GST_BUFFER_DATA(mBuffer), mBufferSize);
            mNewBuffer = false;
            pthread_mutex_unlock(&mMutexBuffer);
            blocking_read = false; // Done, return true.
            return true;
        }
        
        if(timeout > 0) {
            // Check for timeout.
            gettimeofday(&end, NULL);
            seconds  = end.tv_sec  - start.tv_sec;
            useconds = end.tv_usec - start.tv_usec;
            mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
            if(mtime > timeout) {
                LOG_INFO("Timeout reached");
                return false;
            }
        }
    } while (blocking_read);
    return true;
}

bool CamGst::skipBuffer() {
    LOG_DEBUG("CamGst: skipBuffer");
    bool skipped = false;
    pthread_mutex_lock(&mMutexBuffer);
    skipped = mNewBuffer;
    mNewBuffer = false;
    pthread_mutex_unlock(&mMutexBuffer);
    return skipped;
}

bool CamGst::storeImageToFile(std::vector<uint8_t> const& buffer, 
        std::string const& file_name) {
    LOG_DEBUG("CamGst: storeImageToFile, buffer contains %d bytes, stores to %s", 
            buffer.size(), file_name.c_str());

    if(buffer.empty()) {
        LOG_WARN("Empty buffer passed, nothing will be stored");
        return false;
    }

    FILE* file = NULL;
    file = fopen(file_name.c_str(),"w");
    if(file == NULL) {
        LOG_ERROR("File %s could not be opened, no image will be stored", file_name.c_str());
        return false;
    }
  	unsigned int written = fwrite (&buffer[0], 1 , buffer.size(), file);
    if(written != buffer.size()) {
        LOG_ERROR("Only %d of %d bytes could be written", written, buffer.size());
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

// PRIVATE

CamGst::CamGst() {}

void CamGst::setCameraParameters(uint32_t* width, uint32_t* height, uint32_t* fps) {
    LOG_DEBUG("CamGst: setCameraParameters");
    CamConfig config(mDevice);

    // Take the last used values if parameter is 0.
    if(*width == 0) config.getImageWidth(width);
    if(*height == 0) config.getImageHeight(height);
    if(*fps == 0) config.getFPS(fps);

    config.writeImagePixelFormat(*width, *height);
    config.writeFPS(*fps);

    // Get the actual parameters set by the driver.
    config.getImageWidth(width);
    config.getImageHeight(height);
    config.getFPS(fps);
    LOG_INFO("Set camera parameters: width %d, height %d, fps %d", *width, *height, *fps); 
}

GstElement* CamGst::createDefaultSource(std::string const& device) {
    LOG_DEBUG("createDefaultSource, device: %s",device.c_str());
    GstElement* element = gst_element_factory_make ("v4l2src", "default_source");
    if(element == NULL)
        throw CamGstException("Default source could not be created.");
    g_object_set (G_OBJECT (element), 
            "device", device.c_str(), 
            (void*)NULL);
    return element;
}

static std::string toGstreamerMediaType(frame_mode_t mode)
{
    switch(mode)
    {
        case MODE_GRAYSCALE: return "video/x-raw-gray";
        case MODE_RGB:       return "video/x-raw-rgb";
        case MODE_UYVY:      return "video/x-raw-yuv";
        case MODE_JPEG:      return "image/jpeg";
        default:
            throw std::runtime_error("does not know the media type for mode " + boost::lexical_cast<std::string>(mode));
    }
}

static std::string toGstreamerFourCC(frame_mode_t mode)
{
    switch(mode)
    {
        case MODE_GRAYSCALE: return "";
        case MODE_RGB:       return "";
        case MODE_UYVY:      return "UYVY";
        case MODE_JPEG:      return "";
        default:
            throw std::runtime_error("does not know the media type for mode " + boost::lexical_cast<std::string>(mode));
    }
}

// remove format, bb to 24?
GstElement* CamGst::createDefaultCap(uint32_t const width, uint32_t const height, uint32_t const fps, uint32_t bpp, frame_mode_t image_mode ) {
    LOG_DEBUG("createDefaultCap, width: %d, height: %d, fps: %d", width, height, fps);
    GstElement* element = gst_element_factory_make("capsfilter", "default_cap");
    if(element == NULL)
        throw CamGstException("Default cap could not be created.");

    std::string media_type("video/x-raw-yuv"), fourcc;
    if (image_mode != MODE_UNDEFINED)
    {
        media_type = toGstreamerMediaType(image_mode);
        fourcc     = toGstreamerFourCC(image_mode);
    }
    if (image_mode == MODE_GRAYSCALE) {
        bpp = 8;
    }

    GstCaps* caps = gst_caps_new_simple (media_type.c_str(),
            "width", G_TYPE_INT, width,
            "height", G_TYPE_INT, height,
            "framerate", GST_TYPE_FRACTION, fps, 1,
            "bpp", G_TYPE_INT, bpp,
            (void*)NULL);
    if (!fourcc.empty())
    {
        gst_caps_set_simple(caps,
                "format", GST_TYPE_FOURCC, GST_STR_FOURCC (fourcc.c_str()),
                (void*)NULL);
    }

    char* debug_str = gst_caps_to_string(caps);
    LOG_DEBUG("createDefaultCap: %s", debug_str);
    g_free(debug_str);

    // Set property 'caps' of element 'capsfilter'.
    g_object_set (G_OBJECT (element), "caps", caps, (void*)NULL);
    return element;
}

// mode to 1 - streaming?
GstElement* CamGst::createDefaultEncoder(int32_t const jpeg_quality) {
    LOG_DEBUG("CamGst: createDefaultEncoder jpegenc, jpeg_quality: %d", jpeg_quality);
    GstElement* element = gst_element_factory_make ("jpegenc", "default_encoder");
    if(element == NULL)
        throw CamGstException("Default encoder could not be created.");
    g_object_set (G_OBJECT (element),
        "quality", jpeg_quality,
        (void*)NULL);

    return element;
}

// removes max-buffers and drop?
GstElement* CamGst::createDefaultSink() {
    LOG_DEBUG("CamGst: createDefaultSink");
    GstElement* element  = gst_element_factory_make("appsink", "default_buffer_sink");
    if(element == NULL)
        throw CamGstException("Default sink could not be created.");
    g_object_set (G_OBJECT (element), 
            "sync", FALSE, 
            //"max_buffers", 8,
            //"drop", FALSE,
            (void*)NULL);
	gst_app_sink_set_emit_signals ((GstAppSink*) element, TRUE);

    // do after add und link? Disconnecting?
	g_signal_connect (element, "new-buffer",  G_CALLBACK (callbackNewBufferStatic), this);
    return element;
}

bool CamGst::readFileDescriptor(){
    LOG_DEBUG("CamGst: readFileDescriptor");
    if(!mPipelineRunning || mSource == NULL) {
        LOG_WARN("Pipeline is not running or no source available, FD could not be requested.");
        return false;
    }

    // Request and store file descriptor.
    int file_descriptor = -1;
    g_object_get(G_OBJECT (mSource), "device-fd", &file_descriptor, (void*)NULL);
    if(file_descriptor == -1) {
        LOG_ERROR("FD could not be requested.");
        return false;
    }
    mFileDescriptor = file_descriptor;
    return true;   
}

// PRIVATE STATIC 
void* CamGst::mainLoop(void* ptr) {
    LOG_INFO("Start gst main loop");
    GMainLoop* gmain_loop = (GMainLoop*)ptr;
    g_main_loop_run ((GMainLoop*)gmain_loop);
    LOG_INFO("Stop gst main loop");
    return NULL;
}

gboolean CamGst::callbackMessagesStatic(GstBus* bus, GstMessage* msg, gpointer data)
{
    CamGst* cam_gst = (CamGst*)data;
    return cam_gst->callbackMessages(bus, msg, data);
}

gboolean CamGst::callbackMessages(GstBus* bus, GstMessage* msg, gpointer data)
{
    //CamGst* cam_gst = (CamGst*)data;
    LOG_DEBUG("GStreamer callback message: %s", GST_MESSAGE_TYPE_NAME (msg));

    switch (GST_MESSAGE_TYPE (msg)) {

        case GST_MESSAGE_EOS:
            LOG_INFO("GStreamer end of stream reached.");
            //cam_gst->deletePipeline();
        break;

        case GST_MESSAGE_ERROR: {
            gchar  *debug;
            GError *error;

            gst_message_parse_error (msg, &error, &debug);
            g_free (debug);

            LOG_INFO("GStreamer error message received: ", error->message);
            g_error_free (error);
            //cam_gst->deletePipeline();
        break;
        }
        default: break;
    }
    return true;
}

void CamGst::callbackNewBufferStatic(GstElement* object, CamGst* cam_gst_p) {
    cam_gst_p->callbackNewBuffer(object, cam_gst_p);
}   

void CamGst::callbackNewBuffer(GstElement* object, CamGst* cam_gst_p) {
    LOG_DEBUG("CamGst: callbackNewBuffer");
    pthread_mutex_lock(&mMutexBuffer);
    if(mBuffer != NULL) {
        LOG_DEBUG("Unref old image buffer");
        gst_buffer_unref(mBuffer);
        mBuffer = NULL;
        mBufferSize = 0; 
    }
    mBuffer = gst_app_sink_pull_buffer((GstAppSink*)object);

    if(mBuffer == NULL) { // EOS was received before any buffer
        mBufferSize = 0;
        mNewBuffer = false;
        LOG_WARN("EOS was received before any buffer");
    } else {
        mBufferSize = GST_BUFFER_SIZE(mBuffer);
        mNewBuffer = true;
        LOG_DEBUG("New image received, size: %d",mBufferSize); 
    }
    pthread_mutex_unlock(&mMutexBuffer);
} 
} // end namespace camera

