#include "cam_gst.h"

#include <sys/time.h>

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
        mSource(NULL),
        mFileDescriptor(-1),
        mBuffer(NULL),
        mBufferSize(0),
        mNewBuffer(false){
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

void CamGst::createDefaultPipeline(uint32_t width, uint32_t height, uint32_t fps, 
        uint32_t jpeg_quality, bool check_for_valid_params) {
    LOG_DEBUG("CamGst: createDefaultPipeline");
    deletePipeline();

    // Sets the passed parameters if possible, otherwise valid ones.
    if(check_for_valid_params) {
        setCameraParameters(&width, &height, &fps);
    }
    mJpegQuality = jpeg_quality;

    GstElement* source = createDefaultSource(mDevice);
    mSource = source;
    GstElement* cap = createDefaultCap(width, height, fps); // format
    GstElement* encoder = createDefaultEncoder(jpeg_quality);
    GstElement* sink = createDefaultSink();

    if((mPipeline = gst_pipeline_new ("default_pipeline")) == NULL) {
        throw CamGstException("Default pipeline could not be created.");    
    }

    // Add a message handler.
    if(mGstPipelineBus != NULL) {
        gst_object_unref (mGstPipelineBus); 
        mGstPipelineBus = NULL;
    }
    mGstPipelineBus = gst_pipeline_get_bus (GST_PIPELINE (mPipeline));
    gst_bus_add_watch (mGstPipelineBus, callbackMessagesStatic, this);  

    // Add elements to pipeline.
    gst_bin_add_many (GST_BIN (mPipeline), source, cap, encoder, sink, (void*)NULL);

    // Link elements.
    if (!gst_element_link_many (source, cap, encoder, sink, (void*)NULL)) {
	    throw CamGstException("Failed to link default pipeline!");
    } 
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
            if(time_passed_msec > DEFAULT_PIPELINE_TIMEOUT) {
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

bool CamGst::getBuffer(uint8_t** buffer, uint32_t* buf_size, bool blocking_read, 
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
                *buf_size = 0;
                *buffer = NULL;
                pthread_mutex_unlock(&mMutexBuffer);
                LOG_DEBUG("No image available");
                return false;
            } else {
                pthread_mutex_unlock(&mMutexBuffer);
                usleep(50); // blocking: wait
            }
        } else {
            // Copy buffer for return.
            uint8_t* tmp_buffer = (uint8_t*)calloc(mBufferSize, 1);
            memcpy(tmp_buffer, GST_BUFFER_DATA(mBuffer), mBufferSize);
            *buf_size = mBufferSize;
            *buffer = tmp_buffer;
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

void CamGst::storeImageToFile(uint8_t* const buffer, uint32_t const buf_size, 
        std::string const& file_name) {
    LOG_DEBUG("CamGst: storeImageToFile, buffer contains %d bytes, stores to %s", 
            buf_size, file_name.c_str());
    FILE* file = NULL;
    file = fopen(file_name.c_str(),"w");
    if(file == NULL) {
        LOG_ERROR("File %s could not be opened, no image will be stored", file_name.c_str());
        return;
    }
  	fwrite (buffer, 1 , buf_size , file);
    fclose(file);
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

// remove format, bb to 24?
GstElement* CamGst::createDefaultCap(uint32_t const width, uint32_t const height, uint32_t const fps ) {
    LOG_DEBUG("createDefaultCap, width: %d, height: %d, fps: %d", width, height, fps);
    GstElement* element = gst_element_factory_make("capsfilter", "default_cap");
    if(element == NULL)
        throw CamGstException("Default cap could not be created.");
    // Set property 'caps' of element 'capsfilter'.
    g_object_set (G_OBJECT (element), 
            // Create capability.
            "caps", gst_caps_new_simple ("video/x-raw-yuv",
            //"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
            "width", G_TYPE_INT, width,
            "height", G_TYPE_INT, height,
            "framerate", GST_TYPE_FRACTION, fps, 1,
            "bpp", G_TYPE_INT, 24,
            (void*)NULL), 
        (void*)NULL);
    return element;
}

// mode to 1 - streaming?
GstElement* CamGst::createDefaultEncoder(int32_t const jpeg_quality) {
    GstElementFactory* factory = gst_element_factory_find ("dspjpegenc");
    GstElement* element = NULL;
    if(factory != NULL) {
    LOG_DEBUG("createDefaultEncoder dspjpegenc, jpeg_quality: %d", jpeg_quality);
        element = gst_element_factory_create (factory, "default_encoder");
        g_object_set (G_OBJECT (element), 
            "mode", 1, 
            (void*)NULL);
    } else {
        LOG_DEBUG("createDefaultEncoder jpegenc, jpeg_quality: %d", jpeg_quality);
        element = gst_element_factory_make ("jpegenc", "default_encoder");
        g_object_set (G_OBJECT (element), 
            "quality", jpeg_quality, 
            (void*)NULL);
    }
    if(element == NULL)
        throw CamGstException("Default encoder could not be created.");

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

