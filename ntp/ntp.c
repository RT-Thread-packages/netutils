/*
 *
 * (C) 2014 David Lettier.
 * (C) 2017 Armink (armink.ztl@gmail.com)
 *
 * http://www.lettier.com/
 *
 * NTP client.
 *
 * Compiled with gcc version 4.7.2 20121109 (Red Hat 4.7.2-8) (GCC).
 *
 * Tested on Linux 3.8.11-200.fc18.x86_64 #1 SMP Wed May 1 19:44:27 UTC 2013 x86_64 x86_64 x86_64 GNU/Linux.
 * Tested on RT-Thread 3.0.0+
 *
 * To compile: $ gcc main.c -o ntpClient.out
 *
 * Usage: $ ./ntpClient.out
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rtthread.h>

#define NTP_TIMESTAMP_DELTA 2208988800ull
#define NTP_GET_TIMEOUT     10

#ifndef NTP_TIMEZONE_DEAULT
#define NTP_TIMEZONE_DEAULT 8
#endif

#define LI(packet)   (uint8_t) ((packet.li_vn_mode & 0xC0) >> 6) // (li   & 11 000 000) >> 6
#define VN(packet)   (uint8_t) ((packet.li_vn_mode & 0x38) >> 3) // (vn   & 00 111 000) >> 3
#define MODE(packet) (uint8_t) ((packet.li_vn_mode & 0x07) >> 0) // (mode & 00 000 111) >> 0

// Structure that defines the 48 byte NTP packet protocol.
typedef struct {

    uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                         // li.   Two bits.   Leap indicator.
                         // vn.   Three bits. Version number of the protocol.
                         // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;         // Eight bits. Stratum level of the local clock.
    uint8_t poll;            // Eight bits. Maximum interval between successive messages.
    uint8_t precision;       // Eight bits. Precision of the local clock.

    uint32_t rootDelay;      // 32 bits. Total round trip delay time.
    uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;          // 32 bits. Reference clock identifier.

    uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

} ntp_packet;              // Total: 384 bits or 48 bytes.

static ntp_packet packet = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static void error(char* msg)
{
    printf("[NTP]: %s\n", msg); // Print the error message to stderr.
}

void ntp( int argc, char* argv[])
{
    int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.

    int portno = 123; // NTP UDP port number.

    char* host_name = "cn.pool.ntp.org"; // NTP server host-name.

    struct tm time_new;

    // Create and zero out the packet. All 48 bytes worth.

    memset(&packet, 0, sizeof(ntp_packet));

    // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.

    *((char *) &packet + 0) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

    // Create a UDP socket, convert the host-name to an IP address, set the port number,
    // connect to the server, send the packet, and then read in the return packet.

    struct sockaddr_in serv_addr; // Server address data structure.
    struct hostent *server;      // Server data structure.

    sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket.

    if (sockfd < 0) {
        error("ERROR opening socket");
        goto __exit;
    }

    server = gethostbyname(host_name); // Convert URL to IP.

    if (server == NULL) {
        error("ERROR, no such host");
        goto __exit;
    }

    // Zero out the server address structure.

    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;

    // Copy the server's IP address to the server address structure.

    memcpy((char *) &serv_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);

    // Convert the port number integer to network big-endian style and save it to the server address structure.

    serv_addr.sin_port = htons(portno);

    // Call up the server using its IP address and port number.

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
        goto __exit;
    }

    // Send it the NTP packet it wants. If n == -1, it failed.

    n = write(sockfd, (char*) &packet, sizeof(ntp_packet));

    if (n < 0) {
        error("ERROR writing to socket");
        goto __exit;
    }

    fd_set readset;
    struct timeval timeout;
    timeout.tv_sec = NTP_GET_TIMEOUT;
    timeout.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(sockfd, &readset);

    if (select(sockfd + 1, &readset, RT_NULL, RT_NULL, &timeout) <= 0) {
        error("ERROR select the socket timeout(10s)");
        goto __exit;
    }

    // Wait and receive the packet back from the server. If n == -1, it failed.

    n = read(sockfd, (char*) &packet, sizeof(ntp_packet));

    if (n < 0) {
        error("ERROR reading from socket");
        goto __exit;
    }

    // These two fields contain the time-stamp seconds as the packet left the NTP server.
    // The number of seconds correspond to the seconds passed since 1900.
    // ntohl() converts the bit/byte order from the network's to host's "endianness".

    packet.txTm_s = ntohl(packet.txTm_s); // Time-stamp seconds.
    packet.txTm_f = ntohl(packet.txTm_f); // Time-stamp fraction of a second.

    // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
    // Subtract 70 years worth of seconds from the seconds since 1900.
    // This leaves the seconds since the UNIX epoch of 1970.
    // (1900)------------------(1970)**************************************(Time Packet Left the Server)

    time_t txTm = (time_t) (packet.txTm_s - NTP_TIMESTAMP_DELTA);

    // Print the time we got from the server, accounting for local timezone and conversion from UTC time.

    printf("NTP Server Time: %s", ctime((const time_t*) &txTm));

    /* add the timezone offset for set_time/set_date */
    txTm += NTP_TIMEZONE_DEAULT * 3600;

    localtime_r(&txTm, &time_new);

#ifdef RT_USING_RTC
    set_time(time_new.tm_hour, time_new.tm_min, time_new.tm_sec);
    set_date(time_new.tm_year + 1900, time_new.tm_mon + 1, time_new.tm_mday);
#endif

    printf("The system time is updated. Timezone is %d.\n", NTP_TIMEZONE_DEAULT);

__exit:

    close(sockfd);
}
#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(ntp, Update time by NTP(Network Time Protocol));
#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT(ntp, Update time by NTP(Network Time Protocol));
#endif /* FINSH_USING_MSH */
#endif /* RT_USING_FINSH */

