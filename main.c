#include <libusb-1.0/libusb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define VENDOR_ID 0x046d
#define PRODUCT_ID 0xc547

#define INTERFACE_NUMBER 0
#define ENDPOINT_ADDRESS 0x81
#define REPORT_SIZE 13

/*void print_usage(const char *program_name) {*/
/*  fprintf(stderr, "Usage: %s <mouse_device_path>\n", program_name);*/
/*}*/

libusb_device_handle *handle = NULL;
int kernel_driver_detached = 0;
volatile int keep_running = 1;

long long current_timestamp() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000 + (long long)ts.tv_nsec / 1000000;
}

void sigint_handler(int signum) { keep_running = 0; }

void cleanup() {
  if (handle) {
    libusb_release_interface(handle, INTERFACE_NUMBER);

    if (kernel_driver_detached) {
      libusb_attach_kernel_driver(handle, INTERFACE_NUMBER);
      fprintf(stderr, "Kernel driver reattached.\n");
    }

    libusb_close(handle);
  }

  libusb_exit(NULL);
  fprintf(stderr, "Cleanup complete.\n");
}

void print_hid_descriptor(libusb_device_handle *handle, int interface_number) {
  unsigned char desc[256];
  int result = libusb_control_transfer(
      handle,
      LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD |
          LIBUSB_RECIPIENT_INTERFACE,
      LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_REPORT << 8) | 0,
      interface_number, desc, sizeof(desc), 1000);

  if (result < 0) {
    fprintf(stderr, "Failed to get HID descriptor: %s\n",
            libusb_error_name(result));
    return;
  }

  fprintf(stderr, "HID Descriptor (%d bytes):\n", result);

  for (int i = 0; i < result; i++) {
    fprintf(stderr, "%02x ", desc[i]);
    if ((i + 1) % 16 == 0)
      fprintf(stderr, "\n");
  }

  fprintf(stderr, "\n");
}

void print_endpoint_info(libusb_device_handle *handle, int interface_number) {
  struct libusb_config_descriptor *config;
  libusb_device *dev = libusb_get_device(handle);
  libusb_get_config_descriptor(dev, 0, &config);

  for (int i = 0; i < config->interface[interface_number].num_altsetting; i++) {
    const struct libusb_interface_descriptor *intf =
        &config->interface[interface_number].altsetting[i];

    fprintf(stderr, "Interface %d, Alternate Setting %d\n", interface_number,
            i);

    for (int j = 0; j < intf->bNumEndpoints; j++) {
      const struct libusb_endpoint_descriptor *endpoint = &intf->endpoint[j];
      fprintf(stderr, "  Endpoint Address: %02x\n", endpoint->bEndpointAddress);
      fprintf(stderr, "  Attributes: %02x\n", endpoint->bmAttributes);
      fprintf(stderr, "  Max Packet Size: %d\n", endpoint->wMaxPacketSize);
      fprintf(stderr, "  Interval: %d\n", endpoint->bInterval);
    }
  }

  libusb_free_config_descriptor(config);
}

int16_t to_signed_16(uint8_t low, uint8_t high) {
  return (int16_t)((high << 8) | low);
}

int8_t to_signed_8(uint8_t value) {
  return (int8_t)(value & 0x80 ? value - 256 : value);
}

void interpret_report(long long timestamp, unsigned char *data, int length) {
  if (length != REPORT_SIZE) {
    fprintf(stderr, "Unexpected report size: %d\n", length);
    return;
  }

  uint8_t buttons = data[0];
  int16_t x = to_signed_16(data[2], data[3]);
  int16_t y = to_signed_16(data[4], data[5]);
  int8_t wheel = to_signed_8(data[6]);

  printf("%lld %02X %d %d %d\n", timestamp, buttons, x, y, wheel);

  /*printf("Raw data: ");*/
  /**/
  /*for (int i = 0; i < length; i++) {*/
  /*  printf("%02X ", data[i]);*/
  /*}*/
  /**/
  /*printf("\n");*/
  fflush(stdout);
}

int main() {
  unsigned char data[REPORT_SIZE];

  signal(SIGINT, sigint_handler);
  atexit(cleanup);

  libusb_context *context = NULL;
  int result = libusb_init(&context);

  if (result < 0) {
    fprintf(stderr, "Failed to initialize libusb.\n");
    return 1;
  }

  handle = libusb_open_device_with_vid_pid(context, VENDOR_ID, PRODUCT_ID);

  if (handle == NULL) {
    fprintf(stderr, "Could not find/open device.\n");
    libusb_exit(context);
    return 1;
  }

  if (libusb_kernel_driver_active(handle, INTERFACE_NUMBER) == 1) {
    fprintf(stderr, "Kernel driver active, attempting to detach...\n");
    result = libusb_detach_kernel_driver(handle, INTERFACE_NUMBER);

    if (result < 0) {
      fprintf(stderr, "Failed to detach kernel driver: %s\n",
              libusb_error_name(result));
      libusb_close(handle);
      libusb_exit(context);
      return 1;
    }

    kernel_driver_detached = 1;
  }

  result = libusb_claim_interface(handle, INTERFACE_NUMBER);

  if (result < 0) {
    fprintf(stderr, "Failed to claim interface: %s\n",
            libusb_error_name(result));
    libusb_close(handle);
    libusb_exit(context);
    return 1;
  }

  fprintf(stderr, "Listening to raw HIDreports. Press Ctrl+C to stop.\n");

  while (keep_running) {
    int actual_length;
    result = libusb_interrupt_transfer(handle, ENDPOINT_ADDRESS, data,
                                       sizeof(data), &actual_length, 0);

    /*fprintf(stderr, "result: %d  actual_length: %d\n", result, actual_length);*/

    if (result == 0) {
      long long timestamp = current_timestamp();
      interpret_report(timestamp, data, actual_length);
    } else if (result < 0) {
      fprintf(stderr, "Error reading HID report: %s\n",
              libusb_error_name(result));
      break;
    }
  }

  /*print_endpoint_info(handle, INTERFACE_NUMBER);*/
  /*print_hid_descriptor(handle, INTERFACE_NUMBER);*/

  fprintf(stderr, "Exiting...\n");

  return 0;
}
