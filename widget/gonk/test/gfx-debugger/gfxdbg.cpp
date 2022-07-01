/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <binder/Parcel.h>
#include <utils/String8.h>

#define E(format, ...) fprintf(stderr, "E GD: " format "\n", ##__VA_ARGS__)
#define D(format, ...) fprintf(stdout, "D GD: " format "\n", ##__VA_ARGS__)

#define GD_SOCKET_NAME "/dev/socket/gfxdebugger-ipc"
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

using android::Parcel;
using android::String8;

enum {
  GD_CMD_GRALLOC,
  GD_CMD_LAYER,
  GD_CMD_APZ,
};

int sock;
int init_client() {
  socklen_t addrLength;
  struct sockaddr_un address;

  if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    E("socket error: %s", strerror(errno));
    return -1;
  }

  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, GD_SOCKET_NAME);
  address.sun_path[strlen(GD_SOCKET_NAME)] = 0;
  addrLength = sizeof(address.sun_family) + strlen(address.sun_path) + 1;

  if (connect(sock, (struct sockaddr*)&address, addrLength)) {
    E("connect error: %s", strerror(errno));
    return -1;
  }

  return 0;
}

unsigned get_response() {
  Parcel parcel;
  fd_set read_fds;
  uint8_t buffer[128];

  FD_SET(sock, &read_fds);

  select(sock + 1, &read_fds, NULL, NULL, NULL);
  memset(buffer, 0, sizeof(buffer));
  uint32_t size, res = 0;
  if (FD_ISSET(sock, &read_fds)) {
    recv(sock, buffer, sizeof(buffer), 0);
    parcel.setData(buffer, 8);
    size = parcel.readUint32();
    res = parcel.readUint32();
  }

  return res;
}

void end_client() { close(sock); }

/*---------------------------------------------------------------------------*/
enum {
  GRALLOC_OP_LIST,
  GRALLOC_OP_DUMP,
  GRALLOC_OP_DUMP_ALL,
};

int cmd_gralloc(int argc, char** argv) {
  D("cmd_gralloc+, argc=%d, argv[5]=%s", argc, argv[3]);
  Parcel parcel;
  parcel.writeUint32(sizeof(uint32_t));
  parcel.writeUint32(GD_CMD_GRALLOC);

  int c = 0;
  while ((c = getopt(argc, argv, "d:l")) != -1) {
    D("[gralloc] option=%c, arg=%s", c, optarg);
    switch (c) {
      case 'd': {
        parcel.writeUint32(GRALLOC_OP_DUMP);
        unsigned index = strtoul(optarg, NULL, 10);
        parcel.writeUint32(index);
        D("dump gralloc[%d]", index);

        write(sock, parcel.data(), parcel.dataSize());
        int rc;
        fd_set read_fds;
        uint8_t buffer[PATH_MAX];
        memset(buffer, 0, sizeof(buffer));

        FD_SET(sock, &read_fds);
        rc = select(sock + 1, &read_fds, NULL, NULL, NULL);
        if (FD_ISSET(sock, &read_fds)) {
          size_t recv_size = recv(sock, buffer, sizeof(buffer), 0);
          D("received %d bytes", recv_size);
          Parcel p2;
          p2.setData(buffer, recv_size);
          D("parcel size: %d\n", p2.dataSize());
          D("output file: %s\n", p2.readCString());
        }

        break;
      }

      case 'l': {
        parcel.writeUint32(GRALLOC_OP_LIST);
        write(sock, parcel.data(), parcel.dataSize());

        int rc;
        fd_set read_fds;
        uint8_t buffer[PATH_MAX];
        memset(buffer, 0, sizeof(buffer));

        FD_SET(sock, &read_fds);
        rc = select(sock + 1, &read_fds, NULL, NULL, NULL);
        if (FD_ISSET(sock, &read_fds)) {
          size_t recv_size = recv(sock, buffer, sizeof(buffer), 0);
          D("received %d bytes", recv_size);
          Parcel reply;
          reply.setData(buffer, recv_size);
          D("parcel size: %d\n", reply.dataSize());

          uint_t gb_amount = reply.readUint32();
          D("gb_amount: %d\n", gb_amount);
          for (int i = 0; i < gb_amount; i++) {
            D("gralloc: index=%llu", reply.readInt64());
          }
        }

        break;
      }

      default:
        E("Invalid option: -%c\n", optopt);
        break;
    }
  }

  return 0;
}

/*---------------------------------------------------------------------------*/
struct main_cmd {
  const char* name;
  int (*func)(int, char**);
} main_cmds[] = {
    {"gralloc", cmd_gralloc},

    /*
       {"layer", 		cmd_layer},
       {"apz", 		cmd_apz},
     */

};

void usage() {
  printf(
      "usage: \tgfxdbg [OPTION]\n"
      "  -c gralloc\t-l\t\t"
      "list graloc buffers\n"
      "  -c gralloc\t-d\tNUM\t"
      "dump graloc buffers with given id: NUM\n");
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    usage();
    exit(-1);
  }

  init_client();

  int opt = getopt(argc, argv, "c:");
  if (opt == 'c') {
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(main_cmds); i++) {
      D("comparing argv[1]=%s and cmd[%d]=%s", argv[1], i, main_cmds[i].name);
      if (!strcmp(main_cmds[i].name, optarg)) {
        (main_cmds[i].func)(argc, argv);
        break;
      }
    }
    if (i == ARRAY_SIZE(main_cmds)) {
      E("Unknow command: %s\n", argv[2]);
      usage();
      exit(-1);
    }
  }

  end_client();
  return 0;
}
