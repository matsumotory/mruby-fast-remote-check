/*
** mrb_fastremotecheck.c - FastRemoteCheck class
**
** Copyright (c) MATSUMOTO Ryosuke 2017
**
** See Copyright Notice in LICENSE
*/

#include "mruby.h"
#include "mruby/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define SYS_FAIL_MESSAGE_LENGTH 2048
#define DONE mrb_gc_arena_restore(mrb, 0);
#define PACKET_FOUND 1
#define PACKET_NOT_FOUND 0

struct pseudo_ip_header {
  unsigned int src_ip;
  unsigned int dst_ip;
  unsigned char zero;
  unsigned char protocol;
  unsigned short len;
};

struct pseudo_header {
  struct pseudo_ip_header iphdr;
  struct tcphdr ptcphdr;
};

/* mruby-fast-remote-check user data */
typedef struct {
  int saddr_size;
  int tcphdr_size;
  int iphdr_size;
  struct sockaddr *peer_ptr;
  struct tcphdr *tcphdr;
  struct timeval timeout;
} mrb_fastremotecheck_data;

static const struct mrb_data_type mrb_fastremotecheck_data_type = {
    "mrb_fastremotecheck_data", mrb_free,
};

static void mrb_fastremotecheck_sys_fail(mrb_state *mrb, int error_no, const char *fmt, ...)
{
  char buf[2048];
  char arg_msg[SYS_FAIL_MESSAGE_LENGTH];
  char err_msg[SYS_FAIL_MESSAGE_LENGTH];
  char *ret;
  va_list args;

  va_start(args, fmt);
  vsnprintf(arg_msg, SYS_FAIL_MESSAGE_LENGTH, fmt, args);
  va_end(args);

  if ((ret = strerror_r(error_no, buf, sizeof(buf))) == NULL) {
    snprintf(err_msg, SYS_FAIL_MESSAGE_LENGTH, "[BUG] strerror_r failed. errno: %d message: %s", errno, arg_msg);
    mrb_sys_fail(mrb, err_msg);
  }

  snprintf(err_msg, SYS_FAIL_MESSAGE_LENGTH, "sys failed. errno: %d message: %s mrbgem message: %s", error_no, ret,
           arg_msg);
  mrb_sys_fail(mrb, err_msg);
}

static unsigned short checksum(unsigned short *buffer, int size)
{
  unsigned long cksum = 0;
  while (size > 1) {
    cksum += *buffer++;
    size -= sizeof(unsigned short);
  }
  if (size)
    cksum += *(char *)buffer;

  cksum = (cksum >> 16) + (cksum & 0xffff);
  cksum += (cksum >> 16);
  return (unsigned short)(~cksum);
}

static mrb_value mrb_fastremotecheck_init(mrb_state *mrb, mrb_value self)
{
  mrb_fastremotecheck_data *data;
  const char *src_ip;
  const char *dst_ip;
  mrb_int src_port;
  mrb_int dst_port;
  mrb_int timeout_arg = 0;
  struct tcphdr tcphdr;
  struct pseudo_header pheader;
  struct sockaddr_in peer;
  struct timeval timeout;
  timeout.tv_sec = 3; /* default timeout */
  timeout.tv_usec = 0;

  data = (mrb_fastremotecheck_data *)DATA_PTR(self);
  if (data) {
    mrb_free(mrb, data);
  }

  DATA_TYPE(self) = &mrb_fastremotecheck_data_type;
  DATA_PTR(self) = NULL;

  mrb_get_args(mrb, "zizii", &src_ip, &src_port, &dst_ip, &dst_port, &timeout_arg);
  data = (mrb_fastremotecheck_data *)mrb_malloc(mrb, sizeof(mrb_fastremotecheck_data));

  if (timeout_arg) {
    /* for now, support sec only */
    timeout.tv_sec = timeout_arg;
    timeout.tv_usec = 0;
  }

  tcphdr.source = htons(src_port);
  tcphdr.dest = htons(dst_port);
  tcphdr.window = htons(4321);
  tcphdr.seq = 1;
  tcphdr.fin = 0;
  tcphdr.syn = 1;
  tcphdr.doff = 5;
  tcphdr.rst = 0;
  tcphdr.urg = 0;
  tcphdr.urg_ptr = 0;
  tcphdr.psh = 0;
  tcphdr.ack_seq = 0;
  tcphdr.ack = 0;
  tcphdr.check = 0;
  tcphdr.res1 = 0;
  tcphdr.res2 = 0;

  inet_aton(src_ip, (struct in_addr *)&pheader.iphdr.src_ip);
  inet_aton(dst_ip, (struct in_addr *)&pheader.iphdr.dst_ip);

  pheader.iphdr.zero = 0;
  pheader.iphdr.protocol = 6;
  pheader.iphdr.len = htons(sizeof(struct ip));

  inet_aton(dstip, &peer.sin_addr);
  peer.sin_port = htons(dst_port);
  peer.sin_family = AF_INET;

  bcopy((char *)&tcphdr, (char *)&pheader.ptcphdr, sizeof(struct ip));
  tcphdr.check = checksum((unsigned short *)&pheader, 32);

  data->saddr_size = sizeof(peer);
  data->peer_ptr = (struct sockaddr *)&peer;
  data->tcphdr = &tcphdr;
  data->tcphdr_size = sizeof(struct tcphdr);
  data->iphdr_size = sizeof(struct ip);
  data->timeout = timeout;

  DATA_PTR(self) = data;

  return self;
}

static mrb_int fastremotecheck_create_raw_socket(mrb_state *mrb, struct timeval timeout)
{
  mrb_int sock;
  mrb_int ret;

  sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
  if (sock < 0) {
    mrb_fastremotecheck_sys_fail(mrb, errno, "socket failed. need CAP_NET_RAW?");
  }

  ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  if (ret < 0) {
    mrb_fastremotecheck_sys_fail(mrb, errno, "setsockopt SO_RCVTIMEO failed");
  }

  ret = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
  if (ret < 0) {
    mrb_fastremotecheck_sys_fail(mrb, errno, "setsockopt SO_SNDTIMEO failed");
  }

  return sock;
}

static mrb_int fastremotecheck_found_syn_ack_packet(mrb_fastremotecheck_data *data, unsigned char buffer)
{
  struct iphdr *iph = (struct iphdr *)buffer;
  struct tcphdr *tcp = (struct tcphdr *)(buffer + data->iphdr_size);

  if (tcp->syn == 1 && tcp->ack == 1 && tcp->source == data->peer_ptr->sin_port &&
      iph->saddr == data->peer_ptr->sin_addr.s_addr) {
    return PACKET_FOUND;
  } else {
    return PACKET_NOT_FOUND;
  }
}

static mrb_value mrb_fastremotecheck_port_raw(mrb_state *mrb, mrb_value self)
{
  mrb_int sock;
  mrb_int retry = 0;
  mrb_int max_retry = 5; /* default max retry */
  mrb_fastremotecheck_data *data = DATA_PTR(self);
  mrb_int saddr_size = data->saddr_size;

  sock = fastremotecheck_create_raw_socket(mrb, data->timeout);

  ret = sendto(sock, data->tcphdr, data->tcphdr_size, 0, data->peer_ptr, saddr_size);
  if (ret < 0) {
    mrb_fastremotecheck_sys_fail(mrb, errno, "sendto failed");
  }

  while (retry < max_retry) {
    int ret = 0;
    unsigned char buffer[4096] = {0};

    ret = recvfrom(sock, buffer, 4096, 0, data->peer_ptr, &saddr_size);
    if (ret < 0) {
      mrb_fastremotecheck_sys_fail(mrb, errno, "recvfrom failed");
    }

    if (fastremotecheck_found_syn_ack_packet(data, buffer) == PACKET_FOUND) {
      return mrb_true_value();
    }
    retry++:
  }

  close(sock);

  return mrb_false_value();
}

void mrb_mruby_fast_remote_check_gem_init(mrb_state *mrb)
{
  struct RClass *fastremotecheck;
  fastremotecheck = mrb_define_class(mrb, "FastRemoteCheck", mrb->object_class);
  mrb_define_method(mrb, fastremotecheck, "initialize", mrb_fastremotecheck_init, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, fastremotecheck, "raw", mrb_fastremotecheck_port_raw, MRB_ARGS_NONE());
  DONE;
}

void mrb_mruby_fast_remote_check_gem_final(mrb_state *mrb)
{
}
