#include <pebble.h>

Window *window;
TextLayer *text_layer;
char str[100];
DictionaryIterator *iter;

static DataLoggingSessionRef my_data_log;
static bool s_is_logging = false;
static int count = 0;

static void accel_data_handler(AccelData *data, uint32_t num_samples) 
{
  data_logging_log(my_data_log, data, num_samples);  
  count++;
  
  //Change millisecond to readable data time string
  time_t t = data[0].timestamp/1000;
  struct tm * timeinfo = gmtime(&t);
  char timebuffer[64];
  strftime(timebuffer, 64,"%H:%M:%S", timeinfo); //"%Y/%m/%d %H:%M:%S"

  //Text buffer for  showing on the screen
  snprintf(str, 100, "Time: %s \n Count: %d \n x: %d mG \n y: %d mG \n z: %d mG", timebuffer, count, data[0].x, data[0].y, data[0].z);
  text_layer_set_text(text_layer, str);
}

static void toggle_logging() 
{
  if (s_is_logging) 
  {
    s_is_logging = false;
    
    //Notify (power)
    BatteryChargeState power = battery_state_service_peek();
    
    app_message_outbox_begin(&iter);  
    dict_write_int16(iter, 0, 5002);
    dict_write_int16(iter, 1, power.charge_percent);    
    app_message_outbox_send();
    
    // Stop logging
    text_layer_set_text(text_layer, "Not logging");
    accel_data_service_unsubscribe();
    data_logging_finish(my_data_log);
  } 
  else 
  {
    s_is_logging = true;
    
    //Configure and notify (sampling rate, power)    
    BatteryChargeState power = battery_state_service_peek();
    
    app_message_outbox_begin(&iter);  
    dict_write_int16(iter, 0, 5001);
    dict_write_int16(iter, 1, 10);  
    dict_write_int16(iter, 2, power.charge_percent);    
    app_message_outbox_send();

    // Start logging
    my_data_log = data_logging_create(0, DATA_LOGGING_BYTE_ARRAY, sizeof(AccelData), true);  
    count = 0;

    // Start generating data   
    accel_data_service_subscribe(1, accel_data_handler);    
    accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
    text_layer_set_text(text_layer, "Logging...");
  }
}

void out_sent_handler(DictionaryIterator *sent, void *context) {
   // outgoing message was delivered
 }


 void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   // outgoing message failed
 }


 void in_received_handler(DictionaryIterator *received, void *context) {
   // incoming message received
 }


 void in_dropped_handler(AppMessageResult reason, void *context) {
   // incoming message dropped
 }

static void select_click_handler(ClickRecognizerRef recognizer, void *context) 
{
  AccelData data;
  accel_service_peek(&data);
  
  snprintf(str, sizeof str, "Select\n\nSample: (%i, %i, %i)", data.x, data.y, data.z);
  text_layer_set_text(text_layer, str);
  
  app_message_outbox_begin(&iter);
  
  dict_write_int16(iter, 0, 5000);
  dict_write_int16(iter, 1, data.x);
  dict_write_int16(iter, 2, data.y);
  dict_write_int16(iter, 3, data.z);
    
  app_message_outbox_send();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) 
{
  text_layer_set_text(text_layer, "Up");
  
  toggle_logging();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) 
{
  BatteryChargeState power = battery_state_service_peek();
  snprintf(str, sizeof str, "Power: %i%%", power.charge_percent);
  
  text_layer_set_text(text_layer, str);
}

static void click_config_provider(void *context) 
{
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

void init(void) {
	// Create a window and text layer
	window = window_create();
  
	text_layer = text_layer_create(GRect(0, 0, 144, 154));
	
	// Set the text, font, and text alignment
	text_layer_set_text(text_layer, "AccelLog");
	text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
	
	// Add the text layer to the window
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));

	// Push the window
	window_stack_push(window, true);
  
  //Register for click events
  window_set_click_config_provider(window, click_config_provider);  
  
  //Register for communication
	app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  
  const uint32_t inbound_size = 64;
  const uint32_t outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
}

void deinit(void) {
	// Destroy the text layer
	text_layer_destroy(text_layer);
	
	// Destroy the window
	window_destroy(window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}