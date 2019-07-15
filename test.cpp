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

using namespace Hz;

/* RX is input, TX is output */
enum iodev { RX, TX };

/* IIO structs required for streaming */
static bool stop;

static void handle_sig(int sig)
{
	printf("Waiting for process to finish...\n");
	stop = true;
}

int main (int argc, char **argv)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
	// RX and TX sample counters
	size_t nrx = 0;
	size_t ntx = 0;

	// Listen to ctrl+c and ASSERT
	signal(SIGINT, handle_sig);

	printf("* Acquiring IIO context\n");
	Context ctx("network", "192.168.2.1");
	if(ctx.devices.size() == 0) {std::cout << "No devices" << std::endl;}

	printf("* Acquiring AD9361 streaming devices\n");

	Device tx = ctx.devices["cf-ad9361-dds-core-lpc"];
	Device rx = ctx.devices["cf-ad9361-lpc"];

	printf("* Configuring AD9361 for streaming\n");
	ctx.devices["ad9361-phy"].in["voltage0"].attributes["rf_port_select"] = "A_BALANCED";
	//wr_ch_lli(chn, "hardwaregain",     71);
	ctx.devices["ad9361-phy"].out["altvoltage0"].attributes["frequency"] = 2.4_GHz;
    ctx.devices["ad9361-phy"].in["voltage0"].attributes["rf_bandwidth"] = 2_MHz;
	ctx.devices["ad9361-phy"].in["voltage0"].attributes["sampling_frequency"] = 2.5_MHz;


	ctx.devices["ad9361-phy"].out["voltage0"].attributes["rf_port_select"] = "A";
	//wr_ch_lli(chn, "hardwaregain",     10);
	//ctx.devices["ad9361-phy"].out["voltage011111"];
	ctx.devices["ad9361-phy"].out["altvoltage1"].attributes["frequency"] = 2.4_GHz;
    ctx.devices["ad9361-phy"].out["voltage0"].attributes["rf_bandwidth"] = 1.5_MHz;
	ctx.devices["ad9361-phy"].out["voltage0"].attributes["sampling_frequency"] = 2.5_MHz;

	printf("* Initializing AD9361 IIO streaming channels\n");
	printf("* Enabling IIO streaming channels\n");
	rx.in["voltage0"].enable();
	rx.in["voltage1"].enable();
	int samplesize = rx.sample_size();
	tx.out["voltage0"].enable();
	tx.out["voltage1"].enable();

	printf("* Creating non-cyclic IIO buffers with 1 MiS\n");
	Buffer rxbuf(rx, 1024*1024, false);
	Buffer txbuf(tx, 1024*1024, false);

	printf("* Starting IO streaming (press CTRL+C to cancel)\n");
	int a = 0;
	start = std::chrono::system_clock::now();
	while (!stop)
	{
		ssize_t nbytes_rx{};
		ptrdiff_t p_inc;
		ptrdiff_t t_inc;
		// Schedule TX buffer
		ssize_t nbytes_tx = txbuf.push();
		if (nbytes_tx < 0) { printf("Error pushing buf %d\n", (int) nbytes_tx); exit(0); }

		// Refill RX buffer
		//nbytes_rx = rxbuf.refill();
		//if (nbytes_rx < 0) { printf("Error refilling buf %d\n",(int) nbytes_rx); exit(0); }

		// READ: Get pointers to RX buf and read IQ from RX buf port 0
		double noise = 0;
		auto j = txbuf.begin();
		auto start1 = std::chrono::system_clock::now();
		/*for (auto i = rxbuf.begin(); !(i == rxbuf.end()); ++i) {
			noise += pow(((*i).imag() - (*j).imag()), 2);
			noise += pow(((*i).real() - (*j).real()), 2);
			j++;
		}*/
		for (auto i = txbuf.begin(); !(i == txbuf.end()); ++i) {
			*i = std::complex<int16_t>((int)(cos(a / 400.) * 32767/16), (int)(sin(a / 400.) * 32767 /16));
			a++;
        }
        end = std::chrono::system_clock::now();
		double seconds1 = std::chrono::duration_cast<std::chrono::duration<double>>(end - start1).count();
		double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
		printf("\tnoise - %f\n", sqrt(noise / (nbytes_rx / rx.sample_size())));
		// Sample counter increment and status output
		nrx += nbytes_rx / rx.sample_size();
		ntx += nbytes_tx / tx.sample_size();
		std::cout << "\tRX " << (1. * nbytes_tx/1e6) / seconds1 << std::endl;
		std::cout << "\tRX " << (1. * nrx/1e6) / seconds <<" MSmp, TX " << (1. * ntx/1e6) / seconds << " MSmp\n";
	}
	return 0;
}

