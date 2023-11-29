#include <iostream>
#include <string>
#include <gst/gst.h>

bool print_gst_launch_only;
std::string additional_parameters;
std::string video_format;
std::string input_source;
std::string vision_config_file_path;
static gboolean waiting_eos = FALSE;
static gboolean caught_sigint = FALSE;

static void sigint_restore(void)
{
  struct sigaction action;

  memset (&action, 0, sizeof (action));
  action.sa_handler = SIG_DFL;

  sigaction (SIGINT, &action, NULL);
}

/* we only use sighandler here because the registers are not important */
static void
sigint_handler_sighandler(int signum)
{
  /* If we were waiting for an EOS, we still want to catch
   * the next signal to shutdown properly (and the following one
   * will quit the program). */
  if (waiting_eos) {
    waiting_eos = FALSE;
  } else {
    sigint_restore();
  }
  /* we set a flag that is checked by the mainloop, we cannot do much in the
   * interrupt handler (no mutex or other blocking stuff) */
  caught_sigint = TRUE;
}

void add_sigint_handler(void)
{
  struct sigaction action;

  memset(&action, 0, sizeof(action));
  action.sa_handler = sigint_handler_sighandler;

  sigaction(SIGINT, &action, NULL);
}

/* is called every 250 milliseconds (4 times a second), the interrupt handler
 * will set a flag for us. We react to this by posting a message. */
static gboolean check_sigint(GstElement * pipeline)
{
  if (!caught_sigint) {
    return TRUE;
  } else {
    caught_sigint = FALSE;
    waiting_eos = TRUE;
    GST_INFO_OBJECT(pipeline, "handling interrupt. send EOS");
    GST_ERROR_OBJECT(pipeline, "handling interrupt. send EOS");
    gst_element_send_event(pipeline, gst_event_new_eos());

    /* remove timeout handler */
    return FALSE;
  }
}

void print_usage() {
    std::cout << "Basic security camera streaming pipeline usage:" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h --help                  Show this help" << std::endl;
    std::cout << "  --show-fps                 Print fps" << std::endl;
    std::cout << "  --print-gst-launch         Print the ready gst-launch command without running it" << std::endl;
    std::cout << "  -i --input-source          Set the input source (default $DEFAULT_VIDEO_SOURCE)" << std::endl;
    std::cout << "  --vision-config-file-path  Set the vision config file path (default $DEFAULT_VISION_CONFIG_FILE_PATH)" << std::endl;
    exit(0);
}

void parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--print-gst-launch") {
            print_gst_launch_only = true;
        } else if (arg == "--show-fps") {
            std::cout << "Printing fps" << std::endl;
            additional_parameters = "-v | grep -e hailo_display";
        } else if (arg == "-i" || arg == "--input-source") {
            if (i + 1 < argc) {
                input_source = argv[i + 1];
                i++;
            } else {
                std::cout << "Missing input source argument" << std::endl;
                print_usage();
                exit(1);
            }
        } else if (arg == "--vision-config-file-path") {
            if (i + 1 < argc) {
                vision_config_file_path = argv[i + 1];
                i++;
            } else {
                std::cout << "Missing vision config file path argument" << std::endl;
                print_usage();
                exit(1);
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
        } else {
            std::cout << "Received invalid argument: " << arg << ". See expected arguments below:" << std::endl;
            print_usage();
            exit(1);
        }
    }
}

GstFlowReturn wait_for_end_of_pipeline(GstElement *pipeline)
{
    GstBus *bus;
    GstMessage *msg;
    GstFlowReturn ret = GST_FLOW_ERROR;
    bus = gst_element_get_bus(pipeline);
    gboolean done = FALSE;
    // This function blocks until an error or EOS message is received.
    while(!done)
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_MSECOND * 250, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

        if (msg != NULL)
        {
            GError *err;
            gchar *debug_info;
            done = TRUE;
            waiting_eos = FALSE;
            sigint_restore();
            switch (GST_MESSAGE_TYPE(msg))
            {
                case GST_MESSAGE_ERROR:
                {
                    gst_message_parse_error(msg, &err, &debug_info);
                    GST_ERROR("Error received from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);

                    std::string dinfo = debug_info ? std::string(debug_info) : "none";
                    GST_ERROR("Debugging information : %s", dinfo.c_str());

                    g_clear_error(&err);
                    g_free(debug_info);
                    ret = GST_FLOW_ERROR;
                    break;
                }
                case GST_MESSAGE_EOS:
                {
                    GST_INFO("End-Of-Stream reached");
                    ret = GST_FLOW_OK;
                    break;
                }
                default:
                {
                    // We should not reach here because we only asked for ERRORs and EOS
                    GST_WARNING("Unexpected message received %d", GST_MESSAGE_TYPE(msg));
                    ret = GST_FLOW_ERROR;
                    break;
                }
            }
            gst_message_unref(msg);
        }
        check_sigint(pipeline);
    }
    gst_object_unref(bus);
    return ret;
}
std::string buildPipeline(){
    // Basic Directories
    const std::string POSTPROCESS_DIR = "/usr/lib/hailo-post-processes";
    const std::string CROPPING_ALGORITHMS_DIR = POSTPROCESS_DIR + "/cropping_algorithms";
    const std::string RESOURCES_DIR = "/home/root/VHT/resources";

    // Default Video
    const std::string DEFAULT_VIDEO_SOURCE = "/dev/video0";

    const int FOUR_K_BITRATE = 25000000;
    const int FHD_BITRATE = 6000000;
    const int HD_BITRATE = 6000000;
    const int SD_BITRATE = 3000000;

    const int DEFAULT_MAX_BUFFER_SIZE = 5;
    const std::string DEFAULT_FORMAT = "NV12";
    const int DEFAULT_BITRATE = 25000000;
    const std::string DEFAULT_VISION_CONFIG_FILE_PATH = RESOURCES_DIR + "/configs/vision_config.json";
    const std::string DEFAULT_POSTPROCESS_SO = POSTPROCESS_DIR + "/libyolo_post.so";
    const std::string DEFAULT_NETWORK_NAME = "yolov5";
    const std::string DEFAULT_HEF_PATH = RESOURCES_DIR + "/yolov5m_wo_spp_60p_nv12.hef";
    const std::string DEFAULT_JSON_CONFIG_PATH = RESOURCES_DIR + "/configs/yolov5.json";

    input_source = DEFAULT_VIDEO_SOURCE;

    std::string postprocess_so = DEFAULT_POSTPROCESS_SO;
    std::string network_name = DEFAULT_NETWORK_NAME;
    std::string hef_path = DEFAULT_HEF_PATH;
    std::string json_config_path = DEFAULT_JSON_CONFIG_PATH;

    std::string json_config_path_4k = RESOURCES_DIR + "/configs/osd_4k.json";
    std::string json_config_path_fhd = RESOURCES_DIR + "/configs/osd_fhd.json";
    std::string json_config_path_hd = RESOURCES_DIR + "/configs/osd_hd.json";
    std::string json_config_path_sd = RESOURCES_DIR + "/configs/osd_sd.json";

    vision_config_file_path = DEFAULT_VISION_CONFIG_FILE_PATH;

    std::string encoding_hrd = "hrd=false";

    // Limit the encoding bitrate to 10Mbps to support weak host.
    // Comment this out if you encounter a large latency in the host side
    // Tune the value down to reach the desired latency (will decrease the video quality).
    // ----------------------------------------------
    // int bitrate = 10000000;
    // encoding_hrd = "hrd=true hrd-cpb-size=" + std::to_string(bitrate);
    // ----------------------------------------------

    int max_buffers_size = DEFAULT_MAX_BUFFER_SIZE;

    print_gst_launch_only = false;
    additional_parameters = "";
    video_format = DEFAULT_FORMAT;
    std::string sync_pipeline = "false";


    

    // Add the bash commands here
    std::string FPS_DISP = "fpsdisplaysink text-overlay=false sync=" + sync_pipeline + " video-sink=fakesink";
    std::string UDP_SINK = "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                           "rtph264pay config-interval=1 ! application/x-rtp, media=(string)video, encoding-name=(string)H264 ! " + 
                           "udpsink host=10.0.0.2 sync=" + sync_pipeline;
    std::string FOUR_K_TO_ENCODER_BRANCH = "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                                            "hailoosd config-path=" + json_config_path_4k + " ! " + 
                                            "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                                            "hailoh264enc bitrate=" + std ::to_string(FOUR_K_BITRATE) + " hrd=false ! " +
                                            "video/x-h264 ! " +
                                            "tee name=fourk_enc_tee " +
                                            "fourk_enc_tee. ! " +
                                                UDP_SINK + " port=5000 " + 
                                            "fourk_enc_tee. ! " +
                                                "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                                                FPS_DISP + " name=hailo_display_4k_enc ";
    std::string FOUR_K_BRANCH = "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                                FPS_DISP + " name=hailo_display_4k ";
    std::string FHD_BRANCH = "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                              "hailonet hef-path=" + hef_path + " ! " + 
                              "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                              "hailofilter function-name=" + network_name + " config-path=" + json_config_path + " so-path=" + postprocess_so + " qos=false ! " + 
                              "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                              "hailooverlay qos=false ! " +
                              "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                              "hailoosd config-path=" + json_config_path_fhd + " ! " +
                              "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                              "hailoupload pool-size=16 ! " +
                              "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                              "hailoh264enc bitrate=" + std ::to_string(FHD_BITRATE) + " " + encoding_hrd + " ! " +
                              "video/x-h264 ! " +
                              "tee name=fhd_tee " +  
                              "fhd_tee. ! " +
                                   UDP_SINK + " port=5002 " +  
                              "fhd_tee. ! " +
                                  "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                                   FPS_DISP + " name=hailo_display_fhd ";
    std::string HD_BRANCH = "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                             "hailoosd config-path=" + json_config_path_hd + " ! " + 
                             "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                             "hailoh264enc bitrate=" + std ::to_string(HD_BITRATE) + " " + encoding_hrd + " ! " +
                             "video/x-h264 ! " + 
                             "tee name=hd_tee " +
                             "hd_tee. ! " + 
                                UDP_SINK + " port=5004 " + 
                             "hd_tee. ! " +
                                 "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                                 FPS_DISP + " name=hailo_display_hd_enc ";
    std::string SD_BRANCH = "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
                             "hailoosd config-path=" + json_config_path_sd + " ! " +
                             "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                             "hailoh264enc bitrate=" + std ::to_string(SD_BITRATE) + " " + encoding_hrd + " ! " +
                             "video/x-h264 ! " +
                             "tee name=sd_tee " +
                             "sd_tee. ! " +
                                UDP_SINK + " port=5006 " +
                             "sd_tee. ! " +
                                "queue leaky=no max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " + 
                                FPS_DISP + " name=hailo_display_sd_enc ";

    std::string PIPELINE = "gst-launch-1.0 v4l2src io-mode=mmap device=" + input_source + " name=src_0 ! " +
        "video/x-raw, width=3840, height=2160, framerate=30/1, format=" + video_format + " ! " + 
        "queue leaky=downstream max-size-buffers=" + std::to_string(max_buffers_size) + " max-size-bytes=0 max-size-time=0 ! " +
        "hailovisionpreproc config-file-path=" + vision_config_file_path + " name=preproc " +
        "preproc. ! " + FOUR_K_BRANCH + 
        "preproc. ! " + FHD_BRANCH + 
        "preproc. ! " + HD_BRANCH + 
        "preproc. ! " + SD_BRANCH + 
        additional_parameters;

    return PIPELINE;
}

int main(int argc, char* argv[]) {
    add_sigint_handler();
    parse_args(argc, argv);

    std::string str_pipline = buildPipeline();
    if(print_gst_launch_only){
        std::cout << str_pipline << std::endl;
        exit(0);
    }
    // g_setenv("GST_DEBUG", "*:3", TRUE);
    gst_init(&argc, &argv);
    std::cout << "Created pipeline string." << std::endl;
    GstElement *pipeline = gst_parse_launch(str_pipline.c_str(), NULL);

    std::cout << "Parsed pipeline." << std::endl;
    std::cout << "Setting state to playing." << std::endl;
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    GstFlowReturn ret = wait_for_end_of_pipeline(pipeline);
    std::cout << "pipeline endedd." << std::endl;
    
    // Free resources
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_deinit();    
    return ret;
    return 0;
}