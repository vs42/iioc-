/*
 * libiio - AD9361 IIO streaming example
 *
 * Copyright (C) 2014 IABG mbH
 * Author: Michael Feilen <feilen_at_iabg.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 **/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <chrono>

#include <iostream>

#ifdef __APPLE__
#include <iio/iio.h>
#else
#include "iioc++.h"
#endif

/* helper macros */
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))

#define ASSERT(expr) { \
	if (!(expr)) { \
		(void) fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
		(void) abort(); \
	} \
}

/* RX is input, TX is output */
enum iodev { RX, TX };

/* common RX and TX streaming params */
struct stream_cfg {
	long long bw_hz; // Analog banwidth in Hz
	long long fs_hz; // Baseband sample rate in Hz
	long long lo_hz; // Local oscillator frequency in Hz
	const char* rfport; // Port name
	long long x;
};

/* static scratch mem for strings */
static char tmpstr[64];

/* IIO structs required for streaming */
Context *ctx   = NULL;
Channel *rx0_i = NULL;
Channel *rx0_q = NULL;
Channel *tx0_i = NULL;
Channel *tx0_q = NULL;
Buffer  *rxbuf = NULL;
Buffer  *txbuf = NULL;

static bool stop;

static void handle_sig(int sig)
{
	printf("Waiting for process to finish...\n");
	stop = true;
}

/* check return value of attr_write function */
static void errchk(int v, const char* what) {
	 if (v < 0) { fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what); exit(0); }
}

std::string itos (int n) {
    if (n == 0) {
        return "0";
    }
    int d = 1;
    while (d <= n) {
        d *= 10;
    }
    d /= 10;
    std::string s;
    while (d > 0) {
        s += '0' + (n / d);
        n %= d;
        d /= 10;
    }
    return s;
}

/* finds AD9361 streaming IIO devices */
static bool get_ad9361_stream_dev(Context *ctx, enum iodev d, Device **dev)
{
	switch (d) {
	case TX: *dev = ctx->devices["cf-ad9361-dds-core-lpc"]; return *dev != NULL;
	case RX: *dev = ctx->devices["cf-ad9361-lpc"];  return *dev != NULL;
	default: ASSERT(0); return false;
	}
}

/* finds AD9361 streaming IIO channels */
static bool get_ad9361_stream_ch(Context *ctx, enum iodev d, Device *dev, int chid, Channel **chn)
{
    *chn = dev->find_channel("voltage" + itos(chid), d == TX);
	if (!*chn)
		*chn = dev->find_channel("altvoltage" + itos(chid), d == TX);
	return *chn != NULL;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static bool get_phy_chan(Context *ctx, enum iodev d, int chid, Channel **chn)
{
    switch (d) {
	case RX: *chn = ctx->devices["ad9361-phy"]->in["voltage" + itos(chid)]; return *chn != NULL;
	case TX: *chn = ctx->devices["ad9361-phy"]->out["voltage" + itos(chid)];  return *chn != NULL;
	default: ASSERT(0); return false;
	}
}

/* finds AD9361 local oscillator IIO configuration channels */
static bool get_lo_chan(Context *ctx, enum iodev d, Channel **chn)
{
	switch (d) {
	 // LO chan is always output, i.e. true
	case RX: *chn = ctx->devices["ad9361-phy"]->out["altvoltage0"]; return *chn != NULL;
	case TX: *chn = ctx->devices["ad9361-phy"]->out["altvoltage1"]; return *chn != NULL;
	default: ASSERT(0); return false;
	}
}

/* applies streaming configuration through IIO */
bool cfg_ad9361_streaming_ch(Context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
	Channel *chn = NULL;

	// Configure phy and lo channels
	printf("* Acquiring AD9361 phy channel %d\n", chid);
	if (!get_phy_chan(ctx, type, chid, &chn)) {	return false; }
	chn->attributes["rf_port_select"] = cfg->rfport;
	//wr_ch_lli(chn, "hardwaregain",     cfg->x);
    chn->attributes["rf_bandwidth"] = cfg->bw_hz;
	chn->attributes["sampling_frequency"] = cfg->fs_hz;

	// Configure LO channel
	printf("* Acquiring AD9361 %s lo channel\n", type == TX ? "TX" : "RX");
	if (!get_lo_chan(ctx, type, &chn)) { return false; }
	chn->attributes["frequency"] = cfg->lo_hz;
	return true;
}

/* simple configuration and streaming */
int main (int argc, char **argv)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
	// Streaming devices
	Device *tx;
	Device *rx;

	// RX and TX sample counters
	size_t nrx = 0;
	size_t ntx = 0;

	// Stream configurations
	struct stream_cfg rxcfg;
	struct stream_cfg txcfg;

	// Listen to ctrl+c and ASSERT
	signal(SIGINT, handle_sig);

	// RX stream config
	rxcfg.bw_hz = MHZ(2);   // 2 MHz rf bandwidth
	rxcfg.fs_hz = MHZ(2.5);   // 2.5 MS/s rx sample rate
	rxcfg.lo_hz = GHZ(2.4); // 2.5 GHz rf frequency
	rxcfg.x = 71;
	rxcfg.rfport = "A_BALANCED"; // port A (select for rf freq.)

	// TX stream config
	txcfg.bw_hz = MHZ(1.5); // 1.5 MHz rf bandwidth
	txcfg.fs_hz = MHZ(2.5);   // 2.5 MS/s tx sample rate
	txcfg.lo_hz = MHZ(434.15);   // 2.5 GHz rf frequency
	txcfg.x = -10;
	txcfg.rfport = "A"; // port A (select for rf freq.)

	printf("* Acquiring IIO context\n");
	ctx = new Context("network", "192.168.2.1");
	ASSERT(ctx->devices.size() > 0 && "No devices");

	printf("* Acquiring AD9361 streaming devices\n");
	ASSERT(get_ad9361_stream_dev(ctx, TX, &tx) && "No tx dev found");
	ASSERT(get_ad9361_stream_dev(ctx, RX, &rx) && "No rx dev found");

	printf("* Configuring AD9361 for streaming\n");
	ASSERT(cfg_ad9361_streaming_ch(ctx, &rxcfg, RX, 0) && "RX port 0 not found");
	ASSERT(cfg_ad9361_streaming_ch(ctx, &txcfg, TX, 0) && "TX port 0 not found");

	printf("* Initializing AD9361 IIO streaming channels\n");
	ASSERT(get_ad9361_stream_ch(ctx, RX, rx, 0, &rx0_i) && "RX chan i not found");
	ASSERT(get_ad9361_stream_ch(ctx, RX, rx, 1, &rx0_q) && "RX chan q not found");
	ASSERT(get_ad9361_stream_ch(ctx, TX, tx, 0, &tx0_i) && "TX chan i not found");
	ASSERT(get_ad9361_stream_ch(ctx, TX, tx, 1, &tx0_q) && "TX chan q not found");

	printf("* Enabling IIO streaming channels\n");
    rx0_i->enable();
	rx0_q->enable();
	tx0_i->enable();
	tx0_q->enable();

	printf("* Creating non-cyclic IIO buffers with 1 MiS\n");
	rxbuf = new Buffer(rx, 1024*1024, false);
	if (!rxbuf) {
		perror("Could not create RX buffer");
		exit(0);
	}
	txbuf = new Buffer(tx, 1024*1024, false);
	if (!txbuf) {
		perror("Could not create TX buffer");
		exit(0);
	}

	printf("* Starting IO streaming (press CTRL+C to cancel)\n");
	int a = 0;
	while (!stop)
	{
		ssize_t nbytes_rx, nbytes_tx;
		ptrdiff_t p_inc;
		ptrdiff_t t_inc;
		// Schedule TX buffer
		nbytes_tx = txbuf->push();
		if (nbytes_tx < 0) { printf("Error pushing buf %d\n", (int) nbytes_tx); exit(0); }

		// Refill RX buffer
		//nbytes_rx = iio_buffer_refill(rxbuf);
		//if (nbytes_rx < 0) { printf("Error refilling buf %d\n",(int) nbytes_rx); exit(0); }

		// READ: Get pointers to RX buf and read IQ from RX buf port 0
		double noise = 0;
		auto j = txbuf->begin();
		for (auto i = rxbuf->begin(); !(i == rxbuf->end()); ++i) {
			noise += pow(((*i).imag() - (*j).imag()), 2);
			noise += pow(((*i).real() - (*j).real()), 2);
			j++;
		}
		for (auto i = txbuf->begin(); !(i == txbuf->end()); ++i) {
			*i = std::complex<int16_t>((int)(cos(a / 400.) * 32767/16), (int)(sin(a / 400.) * 32767 /16));
			a++;
        }
        end = std::chrono::system_clock::now();
		double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
		printf("\tnoise - %f\n", sqrt(noise / (nbytes_rx / rx->sample_size())));
		// Sample counter increment and status output
		nrx += nbytes_rx / rx->sample_size();
		ntx += nbytes_tx / rx->sample_size();
		std::cout << "\tRX" << (1. * nrx/1e6) << ' ' << seconds <<" MSmp, TX " << (1. * ntx/1e6) / seconds << " MSmp\n";
	}
	return 0;
}

