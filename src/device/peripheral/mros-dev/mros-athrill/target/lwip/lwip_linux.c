// ../athrill/src/device/peripheral/mros-dev/mros-athrill/target/lwip/lwip_linux.c

// 先にシステムヘッダで fd_set / struct timeval / sockaddr 等を定義
#include <sys/types.h>
#include <sys/time.h>     // struct timeval
#include <sys/select.h>   // fd_set, select()
#include <sys/socket.h>   // socket APIs
#include <netinet/in.h>
#include <arpa/inet.h>    // inet_ntop
#include <netdb.h>        // gethostbyname
#include <sys/ioctl.h>    // ioctl
#include <fcntl.h>        // fcntl
#include <unistd.h>       // read/write/close
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include <lwip/sockets.h> // lwIP 側のプロトタイプ

#include "kernel.h"       // OsSaveLockType, os_save_unlock, os_restore_lock

void lwip_init(void)
{
    return;
}

extern int get_tskid(void);

int lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return accept(s, addr, addrlen);
}

int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    return bind(s, name, namelen);
}

int lwip_shutdown(int s, int how)
{
    return shutdown(s, how);
}

int lwip_getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    return getpeername(s, name, namelen);
}

int lwip_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    return getsockname(s, name, namelen);
}

int lwip_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    return getsockopt(s, level, optname, optval, optlen);
}

int lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    return setsockopt(s, level, optname, optval, optlen);
}

int lwip_close(int s)
{
    return close(s);
}

int lwip_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    OsSaveLockType save;
    os_save_unlock(&save);
    int ret = connect(s, name, namelen);
    os_restore_lock(&save);
    return ret;
}

int lwip_listen(int s, int backlog)
{
    return listen(s, backlog);
}

int lwip_recv(int s, void *mem, size_t len, int flags)
{
    OsSaveLockType save;
    os_save_unlock(&save);
    int ret = recv(s, mem, len, flags);
    os_restore_lock(&save);
    return ret;
}

int lwip_read(int s, void *mem, size_t len)
{
    return (int)read(s, mem, len);
}

int lwip_recvfrom(int s, void *mem, size_t len, int flags,
                  struct sockaddr *from, socklen_t *fromlen)
{
    return recvfrom(s, mem, len, flags, from, fromlen);
}

int lwip_send(int s, const void *dataptr, size_t size, int flags)
{
    return (int)send(s, dataptr, size, flags);
}

int lwip_sendto(int s, const void *dataptr, size_t size, int flags,
                const struct sockaddr *to, socklen_t tolen)
{
    return (int)sendto(s, dataptr, size, flags, to, tolen);
}

int lwip_socket(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}

int lwip_write(int s, const void *dataptr, size_t size)
{
    return (int)write(s, dataptr, size);
}

int lwip_select(int maxfdp1,
                fd_set *readset, fd_set *writeset, fd_set *exceptset,
                struct timeval *timeout)
{
    OsSaveLockType save;
    os_save_unlock(&save);
    int ret = select(maxfdp1, readset, writeset, exceptset, timeout);
    os_restore_lock(&save);
    return ret;
}

int lwip_ioctl(int s, long cmd, void *argp)
{
    return ioctl(s, (unsigned long)cmd, argp);
}

int lwip_fcntl(int s, int cmd, int val)
{
    // 最低限：F_SETFL でノンブロッキング制御など
    // それ以外はそのまま渡す（必要に応じて拡張）
    if (cmd == F_SETFL) {
        int flags = fcntl(s, F_GETFL, 0);
        if (flags == -1) return -1;
        return fcntl(s, F_SETFL, (flags & ~O_NONBLOCK) | (val & O_NONBLOCK));
    }
    return fcntl(s, cmd, val);
}

struct hostent *lwip_gethostbyname(const char *name)
{
    return gethostbyname(name);
}

// 簡易版: IPv4 前提の文字列化（lwIP の ip_addr_t を in_addr と互換扱い）
char *ipaddr_ntoa_r(const ip_addr_t *addr, char *buf, int buflen)
{
    if (!addr || !buf || buflen <= 0) {
        return NULL;
    }
    // 多くの環境で ip_addr_t は IPv4 のとき in_addr とサイズ互換
    // 互換でない場合は lwIP の ip_2_ip4() / ip4_addr_get_u32() 等に合わせて直す
    const struct in_addr *in = (const struct in_addr *)addr;
    if (inet_ntop(AF_INET, in, buf, (socklen_t)buflen) == NULL) {
        return NULL;
    }
    return buf;
}
