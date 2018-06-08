/*
** mrb_fastremotecheck.c - FastRemoteCheck class
**
** Copyright (c) MATSUMOTO Ryosuke 2017
**
** See Copyright Notice in LICENSE
*/

#define _GNU_SOURCE

#include "mruby.h"
#include "mruby/data.h"
#include "mruby/error.h"
#include "mruby/class.h"

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
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>

#define SYS_FAIL_MESSAGE_LENGTH 2048
#define DONE mrb_gc_arena_restore(mrb, 0);
#define PACKET_NOT_FOUND 0
#define SYN_ACK_PACKET_FOUND 1
#define RST_PACKET_FOUND 2
#define FLOAT_TO_TIMEVAL(f, t) { t.tv_sec = f; t.tv_usec = (f - (double)t.tv_sec) * 1000000; }
#define TIMEVAL_TO_MSEC(tv) (tv.tv_sec * 1000 + tv.tv_usec)

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
  u_int32_t src_ip;
  u_int32_t dst_ip;
  u_int16_t src_port;
  u_int16_t dst_port;
  socklen_t saddr_size;
  int tcphdr_size;
  int iphdr_size;
  struct sockaddr *peer_ptr;
  struct tcphdr *tcphdr;
  struct timeval timeout;
} mrb_fastremotecheck_data;

typedef struct {
  u_int32_t dst_ip;
  struct sockaddr *peer_ptr;
  struct timeval timeout;
} mrb_icmp_data;

static void mrb_fastremotecheck_data_free(mrb_state *mrb, void *p)
{
  mrb_fastremotecheck_data *data = (mrb_fastremotecheck_data *)p;

  if (data) {
    if (data->tcphdr) {
      mrb_free(mrb, data->tcphdr);
    }
    if (data->peer_ptr) {
      mrb_free(mrb, data->peer_ptr);
    }
    mrb_free(mrb, data);
  }
}

static const struct mrb_data_type mrb_fastremotecheck_data_type = {
    "mrb_fastremotecheck_data", mrb_fastremotecheck_data_free,
};

static void mrb_icmp_data_free(mrb_state *mrb, void *p)
{
  mrb_icmp_data *data = (mrb_icmp_data *)p;

  if (data) {
    if (data->peer_ptr) {
      mrb_free(mrb, data->peer_ptr);
    }
    mrb_free(mrb, data);
  }
}

static const struct mrb_data_type mrb_icmp_data_type = {
    "mrb_icmp_data", mrb_icmp_data_free,
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
  char *src_ip;
  char *dst_ip;
  mrb_int src_port;
  mrb_int dst_port;
  mrb_float timeout_arg = 0;
  struct tcphdr *tcphdr;
  struct pseudo_header *pheader;
  struct sockaddr_in *peer;
  struct timeval timeout;
  timeout.tv_sec = 3; /* default timeout */
  timeout.tv_usec = 0;

  data = (mrb_fastremotecheck_data *)DATA_PTR(self);
  if (data) {
    mrb_free(mrb, data);
  }

  DATA_TYPE(self) = &mrb_fastremotecheck_data_type;
  DATA_PTR(self) = NULL;

  mrb_get_args(mrb, "zizif", &src_ip, &src_port, &dst_ip, &dst_port, &timeout_arg);
  data = (mrb_fastremotecheck_data *)mrb_malloc(mrb, sizeof(mrb_fastremotecheck_data));

  if (timeout_arg) {
    FLOAT_TO_TIMEVAL(timeout_arg, timeout);
  }

  tcphdr = (struct tcphdr *)mrb_malloc(mrb, sizeof(struct tcphdr));
  memset(tcphdr, 0, sizeof(struct tcphdr));
  tcphdr->source = htons(src_port);
  tcphdr->dest = htons(dst_port);
  tcphdr->window = htons(4321);
  tcphdr->seq = 1;
  tcphdr->fin = 0;
  tcphdr->syn = 1;
  tcphdr->doff = 5;
  tcphdr->rst = 0;
  tcphdr->urg = 0;
  tcphdr->urg_ptr = 0;
  tcphdr->psh = 0;
  tcphdr->ack_seq = 0;
  tcphdr->ack = 0;
  tcphdr->check = 0;
  tcphdr->res1 = 0;
  tcphdr->res2 = 0;

  pheader = (struct pseudo_header *)alloca(sizeof(struct pseudo_header));
  memset(pheader, 0, sizeof(struct pseudo_header));
  inet_aton(src_ip, (struct in_addr *)&pheader->iphdr.src_ip);
  inet_aton(dst_ip, (struct in_addr *)&pheader->iphdr.dst_ip);
  pheader->iphdr.zero = 0;
  pheader->iphdr.protocol = 6;
  pheader->iphdr.len = htons(sizeof(struct ip));

  peer = (struct sockaddr_in *)mrb_malloc(mrb, sizeof(struct sockaddr_in));
  memset(peer, 0, sizeof(struct sockaddr_in));
  inet_aton(dst_ip, &peer->sin_addr);
  peer->sin_port = htons(dst_port);
  peer->sin_family = AF_INET;

  bcopy((char *)tcphdr, (char *)&pheader->ptcphdr, sizeof(struct tcphdr));
  tcphdr->check = checksum((unsigned short *)pheader, 32);

  data->src_ip = pheader->iphdr.src_ip;
  data->dst_ip = pheader->iphdr.dst_ip;
  data->src_port = tcphdr->source;
  data->dst_port = tcphdr->dest;
  data->saddr_size = sizeof(struct sockaddr_in);
  data->peer_ptr = (struct sockaddr *)peer;
  data->tcphdr = tcphdr;
  data->tcphdr_size = sizeof(struct tcphdr);
  data->iphdr_size = sizeof(struct ip);
  data->timeout = timeout;

  DATA_PTR(self) = data;

  return self;
}

static mrb_int socket_with_timeout(mrb_state *mrb, int type, int protocol, struct timeval timeout)
{
  mrb_int sock;
  mrb_int ret;

  sock = socket(AF_INET, type, protocol);
  if (sock < 0) {
    mrb_fastremotecheck_sys_fail(mrb, errno, "socket failed. need CAP_NET_RAW?");
  }

  ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  if (ret < 0) {
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "setsockopt SO_RCVTIMEO failed");
  }

  ret = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
  if (ret < 0) {
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "setsockopt SO_SNDTIMEO failed");
  }

  return sock;
}

static mrb_int fastremotecheck_found_syn_ack_packet(mrb_fastremotecheck_data *data, unsigned char *buffer)
{
  struct iphdr *iphdr = (struct iphdr *)buffer;
  struct tcphdr *tcphdr = (struct tcphdr *)(buffer + data->iphdr_size);

  if (tcphdr->syn == 1 && tcphdr->ack == 1 && iphdr->saddr == data->dst_ip && tcphdr->source == data->dst_port) {
    return SYN_ACK_PACKET_FOUND;
  } else if (tcphdr->rst == 1 && iphdr->saddr == data->dst_ip && tcphdr->source == data->dst_port) {
    return RST_PACKET_FOUND;
  } else {
    return PACKET_NOT_FOUND;
  }
}

static mrb_value mrb_fastremotecheck_port_raw(mrb_state *mrb, mrb_value self)
{
  mrb_int sock;
  mrb_int ret;
  mrb_int retry = 0;
  mrb_int max_retry = 5; /* default max retry */
  mrb_fastremotecheck_data *data = DATA_PTR(self);

  sock = socket_with_timeout(mrb, SOCK_RAW, IPPROTO_TCP, data->timeout);

  ret = sendto(sock, data->tcphdr, data->tcphdr_size, 0, data->peer_ptr, data->saddr_size);
  if (ret < 0) {
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "sendto failed");
  }

  while (retry < max_retry) {
    int ret = 0;
    unsigned char buffer[4096] = {0};
    struct sockaddr saddr;
    socklen_t saddr_size = sizeof(saddr);

    ret = recvfrom(sock, buffer, 4096, 0, (struct sockaddr *)&saddr, &saddr_size);
    if (ret < 0) {
      close(sock);
      mrb_fastremotecheck_sys_fail(mrb, errno, "recvfrom failed");
    }

    ret = fastremotecheck_found_syn_ack_packet(data, buffer);
    if (ret == SYN_ACK_PACKET_FOUND) {
      close(sock);
      return mrb_true_value();
    } else if (ret == RST_PACKET_FOUND) {
      close(sock);
      return mrb_false_value();
    }
    retry++;
  }

  close(sock);

  return mrb_false_value();
}

static mrb_value mrb_fastremotecheck_connect_so_linger(mrb_state *mrb, mrb_value self)
{
  int ret;
  int sock;
  struct linger so_linger;
  mrb_fastremotecheck_data *data = DATA_PTR(self);

  so_linger.l_onoff = 1;
  so_linger.l_linger = 0;

  sock = socket_with_timeout(mrb, SOCK_STREAM, IPPROTO_TCP, data->timeout);

  ret = connect(sock, data->peer_ptr, data->saddr_size);
  if (ret < 0) {
    /* Connection refused */
    if (errno == ECONNREFUSED) {
      close(sock);
      return mrb_false_value();
    }
    /* other errno */
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "connect failed");
  }

  ret = setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
  if (ret < 0) {
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "setsockopt failed");
  }

  close(sock);

  return mrb_true_value();
}

void setup_icmphdr(u_int8_t type, u_int8_t code, u_int16_t id, u_int16_t seq, struct icmphdr *icmphdr)
{
  memset(icmphdr, 0, sizeof(struct icmphdr));
  icmphdr->type = type;
  icmphdr->code = code;
  icmphdr->checksum = 0;
  icmphdr->un.echo.id = id;
  icmphdr->un.echo.sequence = seq;
  icmphdr->checksum = checksum((unsigned short *)icmphdr, sizeof(struct icmphdr));
}

static mrb_value mrb_icmp_init(mrb_state *mrb, mrb_value self)
{
  mrb_icmp_data *data;
  char *dst_ip;
  mrb_float timeout_arg = 0;
  struct sockaddr_in *addr;
  struct timeval timeout;
  timeout.tv_sec = 3; /* default timeout */
  timeout.tv_usec = 0;

  data = (mrb_icmp_data *)DATA_PTR(self);
  if (data) {
    mrb_free(mrb, data);
  }

  DATA_TYPE(self) = &mrb_icmp_data_type;
  DATA_PTR(self) = NULL;

  mrb_get_args(mrb, "zf", &dst_ip, &timeout_arg);
  data = (mrb_icmp_data *)mrb_malloc(mrb, sizeof(mrb_icmp_data));

  if (timeout_arg) {
    /* for now, support sec only */
    FLOAT_TO_TIMEVAL(timeout_arg, timeout);
  }

  addr = (struct sockaddr_in *)mrb_malloc(mrb, sizeof(struct sockaddr_in));
  memset(addr, 0, sizeof(struct sockaddr_in));
  addr->sin_addr.s_addr = inet_addr(dst_ip);
  addr->sin_family = AF_INET;

  data->peer_ptr = (struct sockaddr *)addr;
  data->timeout = timeout;
  data->dst_ip = addr->sin_addr.s_addr;

  DATA_PTR(self) = data;

  return self;
}

static mrb_value mrb_icmp_ping(mrb_state *mrb, mrb_value self)
{
  int sock;
  int ret;
  struct icmphdr icmphdr;
  struct iphdr *recv_iphdr;
  struct icmphdr *recv_icmphdr;
  char buf[1500] = {0};
  int epfd, ready;
  struct epoll_event event, events[1];

  mrb_icmp_data *data = (mrb_icmp_data *)DATA_PTR(self);

  sock = socket_with_timeout(mrb, SOCK_RAW, IPPROTO_ICMP, data->timeout);

  setup_icmphdr(ICMP_ECHO, 0, 0, 0, &icmphdr);

  ret = sendto(sock, (char *)&icmphdr, sizeof(icmphdr), 0, data->peer_ptr, sizeof(struct sockaddr_in));
  if (ret < 0) {
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "sendto failed");
  }

  epfd = epoll_create(1);
  if(epfd == -1) {
    close(epfd);
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "epoll_create failed");
  }

  event.data.fd = sock;
  event.events = EPOLLIN;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event) != 0) {
    close(epfd);
    close(sock);
    mrb_fastremotecheck_sys_fail(mrb, errno, "epoll_ctl failed");
  }

  while (1) {
    ready = epoll_wait(epfd, events, 1, TIMEVAL_TO_MSEC(data->timeout));
    if (ready < 0) {
      close(epfd);
      close(sock);
      mrb_fastremotecheck_sys_fail(mrb, errno, "epoll_wait failed");
    }

    if (ready == 0) {
      close(epfd);
      close(sock);
      mrb_fastremotecheck_sys_fail(mrb, errno, "epoll_wait timeout");
    }    

    memset(&buf, 0, sizeof(buf));
    ret = recv(sock, buf, sizeof(buf), MSG_DONTWAIT);
    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      }
      close(epfd);
      close(sock);
      mrb_fastremotecheck_sys_fail(mrb, errno, "recv failed");
    } else {
      recv_iphdr = (struct iphdr *)buf;
      recv_icmphdr = (struct icmphdr *)(buf + (recv_iphdr->ihl << 2));

      if (data->dst_ip == recv_iphdr->saddr && recv_icmphdr->type == ICMP_ECHOREPLY) {
        close(epfd);
        close(sock);
        return mrb_true_value();
      }
      continue;
    }
  }
}

void mrb_mruby_fast_remote_check_gem_init(mrb_state *mrb)
{
  struct RClass *fastremotecheck;
  struct RClass *tcp;
  struct RClass *icmp;

  fastremotecheck = mrb_define_class(mrb, "FastRemoteCheck", mrb->object_class);
  MRB_SET_INSTANCE_TT(fastremotecheck, MRB_TT_DATA);

  mrb_define_method(mrb, fastremotecheck, "initialize", mrb_fastremotecheck_init, MRB_ARGS_REQ(5));
  mrb_define_method(mrb, fastremotecheck, "open_raw?", mrb_fastremotecheck_port_raw, MRB_ARGS_NONE());
  mrb_define_method(mrb, fastremotecheck, "connectable?", mrb_fastremotecheck_connect_so_linger, MRB_ARGS_NONE());
  DONE;

  tcp = mrb_define_class_under(mrb, fastremotecheck, "TCP", mrb->object_class);
  MRB_SET_INSTANCE_TT(tcp, MRB_TT_DATA);
  mrb_define_method(mrb, tcp, "initialize", mrb_fastremotecheck_init, MRB_ARGS_REQ(5));
  mrb_define_method(mrb, tcp, "open_raw?", mrb_fastremotecheck_port_raw, MRB_ARGS_NONE());
  mrb_define_method(mrb, tcp, "connectable?", mrb_fastremotecheck_connect_so_linger, MRB_ARGS_NONE());
  DONE;

  icmp = mrb_define_class_under(mrb, fastremotecheck, "ICMP", mrb->object_class);
  MRB_SET_INSTANCE_TT(icmp, MRB_TT_DATA);
  mrb_define_method(mrb, icmp, "initialize", mrb_icmp_init, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, icmp, "ping?", mrb_icmp_ping, MRB_ARGS_NONE());

  DONE;
}

void mrb_mruby_fast_remote_check_gem_final(mrb_state *mrb)
{
}
