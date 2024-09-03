#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

void print_usage(const char *program_name) {
  fprintf(stderr, "Usage: %s <mouse_device_path>\n", program_name);
}

long long current_timestamp() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000 + (long long)ts.tv_nsec / 1000000;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  int fd;
  struct input_event ie;

  const char *MOUSE_DEVICE = argv[1];
  fd = open(MOUSE_DEVICE, O_RDONLY);

  if (fd == -1) {
    perror("Error opening device.");
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "Listening to mouse events on %s. Press Ctrl+C to stop.\n",
          MOUSE_DEVICE);

  while (1) {
    if (read(fd, &ie, sizeof(struct input_event)) == -1) {
      perror("Error reading device.");
      break;
    }

    if (ie.type != EV_REL && ie.type != EV_KEY)
      continue;

    long long timestamp = current_timestamp();
    printf("%lld ", timestamp);

    if (ie.type == EV_REL) {
      if (ie.code == REL_X) {
        printf("X %d\n", ie.value);
      } else if (ie.code == REL_Y) {
        printf("Y %d\n", ie.value);
      }
    } else if (ie.type == EV_KEY) {
      switch (ie.code) {
      case BTN_LEFT:
        printf("LEFT %s\n", ie.value ? "DOWN" : "UP");
        break;
      case BTN_RIGHT:
        printf("RIGHT %s\n", ie.value ? "DOWN" : "UP");
        break;
      case BTN_MIDDLE:
        printf("MIDDLE %s\n", ie.value ? "DOWN" : "UP");
      }
    }

    fflush(stdout);
  }

  close(fd);
  return 0;
}
