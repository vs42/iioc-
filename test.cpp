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
	if(ctx->devices.size() == 0) {std::cout << "No devices" << std::endl;}

	printf("* Acquiring AD9361 streaming devices\n");

	tx = ctx->devices["cf-ad9361-dds-core-lpc"];
	rx = ctx->devices["cf-ad9361-lpc"];

	printf("* Configuring AD9361 for streaming\n");
	ctx->devices["ad9361-phy"]->in["voltage0"]->attributes["rf_port_select"] = rxcfg.rfport;
	//wr_ch_lli(chn, "hardwaregain",     cfg->x);
    ctx->devices["ad9361-phy"]->in["voltage0"]->attributes["rf_bandwidth"] = rxcfg.bw_hz;
	ctx->devices["ad9361-phy"]->in["voltage0"]->attributes["sampling_frequency"] = rxcfg.fs_hz;
	ctx->devices["ad9361-phy"]->out["voltage0"]->attributes["rf_port_select"] = txcfg.rfport;
	//wr_ch_lli(chn, "hardwaregain",     cfg->x);
    ctx->devices["ad9361-phy"]->out["voltage0"]->attributes["rf_bandwidth"] = txcfg.bw_hz;
	ctx->devices["ad9361-phy"]->out["voltage0"]->attributes["sampling_frequency"] = txcfg.fs_hz;

	printf("* Initializing AD9361 IIO streaming channels\n");
	rx0_i = rx->find_channel("voltage0", 0);
	if (!rx0_i)
		rx0_i = rx->find_channel("altvoltage0", 0);
	rx0_q = rx->find_channel("voltage1", 0);
	if (!rx0_q)
		rx0_q = rx->find_channel("altvoltage1", 0);
	tx0_i = tx->find_channel("voltage0", 1);
	if (!tx0_i)
		tx0_i = tx->find_channel("altvoltage0", 1);
	tx0_q = tx->find_channel("voltage1", 1);
	if (!tx0_q)
		tx0_q = tx->find_channel("altvoltage1", 1);

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
	start = std::chrono::system_clock::now();
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
		std::cout << "\tRX " << (1. * nrx/1e6) / seconds <<" MSmp, TX " << (1. * ntx/1e6) / seconds << " MSmp\n";
	}
	return 0;
}

