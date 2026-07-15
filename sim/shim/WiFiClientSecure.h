// mbedTLS-backed TLS client implementing the Arduino Client interface, so the
// real bambu_mqtt.cpp + PubSubClient run unchanged on Android. LAN-only: the
// printer serves a self-signed cert, so certificate verification is disabled
// (setInsecure), exactly like the ESP32 WiFiClientSecure build. All methods are
// defined in-class (this header is included by a single TU: bambu_mqtt.cpp).
#pragma once
#ifndef ANDROID_WIFICLIENTSECURE_H
#define ANDROID_WIFICLIENTSECURE_H

#include "Client.h"
#include <cstring>
#include <cstdio>
#include <sys/socket.h>
#include <sys/time.h>

#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>

class WiFiClientSecure : public Client {
public:
    WiFiClientSecure() { init_ctx(); }
    ~WiFiClientSecure() { stop(); free_ctx(); }

    void setInsecure() { _insecure = true; }

    int connect(const char *host, uint16_t port) {
        stop();            // drop any previous session/socket
        reset_session();   // fresh ssl + conf for this handshake

        char portbuf[8];
        snprintf(portbuf, sizeof(portbuf), "%u", (unsigned)port);
        int ret = mbedtls_net_connect(&_net, host, portbuf, MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) { log_err("net_connect", ret); return 0; }

        if ((ret = mbedtls_ssl_config_defaults(&_conf, MBEDTLS_SSL_IS_CLIENT,
                MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
            log_err("config_defaults", ret); stop(); return 0;
        }
        mbedtls_ssl_conf_authmode(&_conf,
            _insecure ? MBEDTLS_SSL_VERIFY_NONE : MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_rng(&_conf, mbedtls_ctr_drbg_random, &_drbg);
        // Bambu's LAN MQTT broker speaks TLS 1.2; pin it (matches bambu_ftp.cpp).
        mbedtls_ssl_conf_min_tls_version(&_conf, MBEDTLS_SSL_VERSION_TLS1_2);
        mbedtls_ssl_conf_max_tls_version(&_conf, MBEDTLS_SSL_VERSION_TLS1_2);

        if ((ret = mbedtls_ssl_setup(&_ssl, &_conf)) != 0) { log_err("ssl_setup", ret); stop(); return 0; }
        mbedtls_ssl_set_hostname(&_ssl, host);   // SNI; harmless with verify off
        mbedtls_ssl_set_bio(&_ssl, &_net, mbedtls_net_send, mbedtls_net_recv, NULL);

        while ((ret = mbedtls_ssl_handshake(&_ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                log_err("handshake", ret); stop(); return 0;
            }
        }

        // Backstop: PubSubClient polls via available(), but keep blocking recv
        // from hanging forever if the peer goes away mid-read.
        struct timeval tv; tv.tv_sec = 10; tv.tv_usec = 0;
        setsockopt(_net.fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

        _connected = true;
        _rpos = _rlen = 0;
        return 1;
    }

    int connect(IPAddress ip, uint16_t port) {
        char host[16];
        snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        return connect(host, port);
    }

    size_t write(const uint8_t *buf, size_t size) {
        if (!_connected) return 0;
        size_t sent = 0;
        while (sent < size) {
            int ret = mbedtls_ssl_write(&_ssl, buf + sent, size - sent);
            if (ret > 0) { sent += (size_t)ret; continue; }
            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            log_err("ssl_write", ret); _connected = false; break;
        }
        return sent;
    }
    size_t write(uint8_t b) { return write(&b, 1); }

    // Non-blocking: refills the decrypt buffer with one record if TCP data is
    // ready, else returns 0. PubSubClient drives all reads through this.
    int available() {
        if (_rpos < _rlen) return _rlen - _rpos;
        if (!_connected) return 0;
        mbedtls_net_set_nonblock(&_net);
        int ret = mbedtls_ssl_read(&_ssl, _rbuf, sizeof(_rbuf));
        mbedtls_net_set_block(&_net);
        if (ret > 0) { _rpos = 0; _rlen = ret; return _rlen; }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
        _connected = false;   // 0, close-notify, or hard error
        return 0;
    }

    int read() {
        if (_rpos >= _rlen) { if (available() <= 0) return -1; }
        return (_rpos < _rlen) ? _rbuf[_rpos++] : -1;
    }
    int read(uint8_t *buf, size_t size) {
        size_t got = 0;
        while (got < size) {
            if (_rpos >= _rlen) { if (available() <= 0) break; }
            while (got < size && _rpos < _rlen) buf[got++] = _rbuf[_rpos++];
        }
        return (int)got;
    }
    int peek() {
        if (_rpos >= _rlen) { if (available() <= 0) return -1; }
        return (_rpos < _rlen) ? _rbuf[_rpos] : -1;
    }
    void flush() {}

    void stop() {
        if (_net.fd != -1) {
            if (_connected) mbedtls_ssl_close_notify(&_ssl);
            mbedtls_net_free(&_net);   // closes fd, resets it to -1
        }
        _connected = false;
        _rpos = _rlen = 0;
    }
    uint8_t connected() { return _connected ? 1 : 0; }
    operator bool() { return _connected; }

private:
    mbedtls_net_context      _net;
    mbedtls_ssl_context      _ssl;
    mbedtls_ssl_config       _conf;
    mbedtls_ctr_drbg_context _drbg;
    mbedtls_entropy_context  _entropy;
    bool _insecure = false;
    bool _connected = false;
    unsigned char _rbuf[1600];
    int _rpos = 0, _rlen = 0;

    void init_ctx() {
        mbedtls_net_init(&_net);
        mbedtls_ssl_init(&_ssl);
        mbedtls_ssl_config_init(&_conf);
        mbedtls_ctr_drbg_init(&_drbg);
        mbedtls_entropy_init(&_entropy);
        const char *pers = "filatrack-tls";
        mbedtls_ctr_drbg_seed(&_drbg, mbedtls_entropy_func, &_entropy,
                              (const unsigned char *)pers, strlen(pers));
    }
    void reset_session() {
        mbedtls_ssl_free(&_ssl);
        mbedtls_ssl_config_free(&_conf);
        mbedtls_ssl_init(&_ssl);
        mbedtls_ssl_config_init(&_conf);
    }
    void free_ctx() {
        mbedtls_ssl_free(&_ssl);
        mbedtls_ssl_config_free(&_conf);
        mbedtls_ctr_drbg_free(&_drbg);
        mbedtls_entropy_free(&_entropy);
    }
    void log_err(const char *where, int ret) {
        char buf[128];
        mbedtls_strerror(ret, buf, sizeof(buf));
        Serial.printf("TLS %s failed: -0x%04X %s\n", where, (unsigned)(-ret), buf);
    }
};

#endif
