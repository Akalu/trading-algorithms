/***************************************************************************
 *   Copyright (C) 2008 by Aliaksei Kaliutau                               *
 *   k5771k@gmail.com                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <cstdlib>
#include <malloc.h>
#include <stdio.h>

using namespace std;

#define GAIN_DEAL 1
#define LOSS_DEAL 2
#define NO_DEAL 3

int sigcount;
int barcount[24];

typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned int DWORD;
typedef unsigned int LONG;

typedef struct
{
	LONG tick;
	float open;
	float high;
	float low;
	float close;
	LONG vol;
} BAR;

typedef struct
{
	LONG tick;
	int nbars;
	float open[24];
	float high[24];
	float low[24];
	float close[24];
	LONG vol;
} BAR24;

typedef struct
{
	int ID;
	BAR *pinstr;
	BAR24 *pinstr24h;
	LONG starttick;
	int length;
	int Tf;
	float spread;
	float dx;
	float tprofit;
	float sloss;
} INSTRUMENT;

typedef struct
{
	int totaldeals;
	int profitdeals, lossdeals;
	int lastdealstat;
	int gainchains, losschains;
	int lostalldays, gainalldays, mediumdays;
	float maxprofit, maxloss;
	float profit;
	float loss;
	float startcap;
	bool dealsignature[12];
	float change;
	float lastcap, maxcap, maxfall;
	float cap;
} STATINFO;

int memallocated;

void *SOR_malloc(int size)
{
	void *memblock;

	memblock = (void *)malloc(size);

	if (memblock == NULL)
	{
		printf("Zero-sized block!\n");
		return NULL;
	}

	return (void *)((char *)memblock);
}

void SOR_free(void *ptr)
{
	void *memblock;

	if (ptr == NULL)
		return;

	memblock = (void *)((char *)ptr);

	free(memblock);
}

char *lexem = new char[4096];
const char* dir = "./data/";

bool LoadData(char *filename, BAR *buf, int *size)
{
	FILE *fhandle;
	BYTE sl;
	bool gotheader, out;
	int offset = 0;

	const size_t path_size = strlen(dir) + strlen(filename) + 1;
	char* path = malloc(path_size);
	snprintf(path, path_size, "%s%s", dir, filename);

	if ((fhandle = fopen(path, "rb")) == NULL)
	{
		printf("Failed to open file %s\n", filename);
		return (false);
	}
	int wd;

	printf("Searching header...\n");
	fread(lexem, 2591, 1, fhandle);// skip header
	printf("Found\n");
	printf("Loading data...\n");
	out = false;
	while (offset < 5 * 365 * 24 && !out)
	{
		if (fread(&(buf[offset++]), sizeof(BAR), 1, fhandle) == EOF)
			out = true;
	};
	*size = offset;
	fclose(fhandle);
	return (true);
}

/*
  This method is used to convert hours bars into the day bar

  hours - input hours data
  days  - output days data 
  start - start tick 
  end   - end tick 
  sized - size of data 
*/
void convh2d(BAR *hours, BAR *days, int start, int end, int *sized)
{
	int i = 0, j = 0, k, id = (end - start) / 86400, nextdaystart = start;
	float o, h, l, c;
	bool reset = true;

	if (reset)
	{
		while (hours[i].tick < nextdaystart)
		{
			i++;
		}
	}

	while (hours[i].tick < end && j < id)
	{
		nextdaystart += 86400;
		o = hours[i++].open;
		h = o;
		l = o;
		reset = false;
		for (k = 0; k < 23; k++)
		{
			if ((hours[i].tick > nextdaystart))
				reset = true;

			if (!reset)
			{
				if (hours[i].high > h)
					h = hours[i].high;
				if (hours[i].low < l)
					l = hours[i].low;
				i++;
			}
			else
			{
				k = 24;
			}
		}
		c = hours[i - 1].close;
		days[j].open = o;
		days[j].high = h;
		days[j].low = l;
		days[j].close = c;
		j++;
	}
	*sized = j;
}
void convh224h(BAR *hours, BAR24 *h24, int start, int end)
{
	int i = 0, j = 0, k, p, w, id = (end - start) / 86400, nextdaystart = start, count4h = 6;
	float o, h, l, c;
	bool reset = true;

	if (reset)
		while (hours[i].tick < nextdaystart)
		{
			i++;
		}

	while (hours[i].tick < end && j < id)
	{
		nextdaystart += 86400;
		h24[j].nbars = 0;
		w = 0;
		for (p = 0; p < 24; p++)
		{
			o = hours[i].open;
			h = hours[i].high;
			l = hours[i].low;
			c = hours[i].close;
			i++;
			reset = false;
			h24[j].open[p] = o;
			h24[j].high[p] = h;
			h24[j].low[p] = l;
			h24[j].close[p] = c;
			h24[j].nbars++;
			if ((hours[i].tick > nextdaystart))
				reset = true;
			if (reset)
				p = 24;
		}
		j++;
	}
}


/*
 Calculates moving averages for hour charts, simple or weighted ones
 type: 0 - simple
       1 -  weight (coeff=0.8)
*/
float movh(BAR24 *h4s, int iday, int ibar, int period, char type, float fweight)
{
	float fmov1 = 0;
	int i = period, cday = iday, j;
	if (type == 0)
	{
		j = ibar;
		while (i != 0)
		{
			if (j < 0)
			{
				cday--;
				j = h4s[cday].nbars - 1;
			}
			fmov1 += h4s[cday].close[j];
			j--;
			i--;
		}
		fmov1 /= period;
		return fmov1;
	}

	if (type == 1)
	{
		float w = 1.0, s = 0.0;
		j = ibar;
		while (i != 0)
		{
			if (j < 0)
			{
				cday--;
				j = h4s[cday].nbars - 1;
			}
			fmov1 += w * h4s[cday].close[j];
			s += w;
			w *= fweight;
			j--;
			i--;
		}
		fmov1 /= s;
		return fmov1;
	}
}

/*
 Calculates moving averages for day charts, simple or weighted ones
 type: 0 - simple
       1 -  weight (coeff=0.8)
*/
float mov(BAR *days, int iday, int period, char type, float fweight)
{
	float fmov1 = 0;
	int i, j;
	if (type == 0)
	{
		for (i = 0; i < period; i++)
			fmov1 += days[iday - period + i].close;
		fmov1 /= period;
		return fmov1;
	}
	if (type == 1)
	{
		float w = 1.0, s = 0.0;
		for (i = 0; i < period; i++)
		{
			fmov1 += w * days[iday - period + i].close;
			s += w;
			w *= fweight;
		}
		fmov1 /= s;
		return fmov1;
	}
}

float fabs(float a)
{
	return ((a >= 0) ? a : -a);
}

float fmax(float a, float b)
{
	return (a >= b) ? a : b;
}

float fmin(float a, float b)
{
	return (a < b) ? a : b;
}

float fmax3(float a, float b, float c)
{
	if (fmax(a, b) < c)
		return c;
	if (fmax(a, c) < b)
		return b;
	return c;
}

float fmin3(float a, float b, float c)
{
	if (fmin(a, b) > c)
		return c;
	if (fmin(a, c) > b)
		return b;
	return c;
}

float fdist(float a, float b, float c)
{
	return (fmax3(fabs(a - b), fabs(a - c), fabs(c - b)));
}

// startdate=1161982800 0x45427350  (27.10.2006 00.00 GMT=1161910800)
// lastdate=1213995600  0x485C1A50
// delta=52012800 (602 days)
void cuptrend2(INSTRUMENT *inst, STATINFO *stat, int nday, float enter)
{
	float dup, ddown, dup1, ddown1, changeofcap, fmov8, fmov30, fmov20, fmov120, enter2, eps = inst->spread;
	float fmov81, f0, f1, f2, df, df30, fmov8_1, fmov30_1, fmov20_1, closep;
	int i = 3, nbars = inst->pinstr24h[nday].nbars, nbars4_1 = inst->pinstr24h[nday - 1].nbars, nbarss;
	bool signal = false;

	nbarss = 12;

	dup1 = inst->pinstr24h[nday].open[i];
	ddown1 = dup1;

	fmov8_1 = movh(inst->pinstr24h, nday, 0, inst->Tf, 1, 0.72);
	fmov20_1 = movh(inst->pinstr24h, nday, 0, 20, 1, 0.91);
	fmov30_1 = movh(inst->pinstr24h, nday, 0, 30, 1, 0.93);

	while (!signal)
	{
		fmov8 = movh(inst->pinstr24h, nday, i, inst->Tf, 1, 0.72);
		fmov20 = movh(inst->pinstr24h, nday, i, 20, 1, 0.91);
		fmov30 = movh(inst->pinstr24h, nday, i, 30, 1, 0.93);
		fmov120 = movh(inst->pinstr24h, nday, i, 120, 1, 0.98);

		{

			if (dup1 < inst->pinstr24h[nday].high[i])
				dup1 = inst->pinstr24h[nday].high[i];
			if (ddown1 > inst->pinstr24h[nday].low[i])
				ddown1 = inst->pinstr24h[nday].low[i];
			if (fdist(inst->pinstr24h[nday].close[i - 1], inst->pinstr24h[nday].close[i - 2], inst->pinstr24h[nday].close[i - 3]) < 2.6 * eps &&
				inst->pinstr24h[nday].close[i] > (fmax3(inst->pinstr24h[nday].close[i - 1], inst->pinstr24h[nday].close[i - 2], inst->pinstr24h[nday].close[i - 3])) && fabs(inst->pinstr24h[nday].close[i] - fmov30) < 2.4 * eps && inst->pinstr24h[nday].close[i] > fmov30)
				signal = true;
		}
		i++;
		fmov8_1 = fmov8;
		fmov20_1 = fmov20;
		fmov30_1 = fmov30;
		if (i >= nbarss)
			return;
	}
	enter2 = inst->pinstr24h[nday].open[i];
	inst->sloss = 1.2 * (inst->pinstr24h[nday].open[i] - fmin3(inst->pinstr24h[nday].low[i - 1], inst->pinstr24h[nday].low[i - 2], inst->pinstr24h[nday].low[i - 3]));
	if (inst->sloss > 9 * eps)
		inst->sloss = 9 * eps;
	barcount[i - 1]++;
	dup = enter2;
	ddown = enter2;
	signal = false;
	closep = inst->pinstr24h[nday].close[nbars - 1];
	while (i < nbars)
	{
		fmov8 = movh(inst->pinstr24h, nday, i, inst->Tf, 1, 0.72);
		fmov20 = movh(inst->pinstr24h, nday, i, 20, 1, 0.91);
		fmov30 = movh(inst->pinstr24h, nday, i, 30, 1, 0.93);
		if (dup < inst->pinstr24h[nday].high[i])
			dup = inst->pinstr24h[nday].high[i];
		if (ddown > inst->pinstr24h[nday].low[i])
			ddown = inst->pinstr24h[nday].low[i];
		i++;
	}
	// results
	dup = dup - enter2 - inst->spread;
	ddown = -(ddown - enter2 + inst->spread);
	// verify the stoploss
	if (ddown >= inst->sloss)
	{
		changeofcap = inst->dx * inst->sloss;
		stat->cap -= changeofcap;
		stat->change -= changeofcap;
		stat->dealsignature[sigcount] = false;
		stat->loss += changeofcap;
		if (stat->maxloss < changeofcap)
			stat->maxloss = changeofcap;
		stat->lossdeals++;
		return;
	}
	// verify for takeprofit
	if (dup >= inst->tprofit)
	{
		changeofcap = inst->dx * inst->tprofit;
		stat->cap += changeofcap;
		stat->change += changeofcap;
		stat->dealsignature[sigcount] = true;
		stat->profit += changeofcap;
		if (stat->maxprofit < changeofcap)
			stat->maxprofit = changeofcap;
		stat->profitdeals++;
		return;
	}
	// close at the end of the day
	changeofcap = inst->dx * (closep - enter2 - inst->spread);
	stat->cap += changeofcap;
	if (changeofcap > 0)
	{
		if (stat->maxprofit < changeofcap)
			stat->maxprofit = changeofcap;
		stat->change += changeofcap;
		stat->dealsignature[sigcount] = true;
		stat->profit += changeofcap;
		stat->profitdeals++;
	}
	else
	{
		if (stat->maxloss < changeofcap)
			stat->maxloss = changeofcap;
		stat->change += changeofcap;
		stat->dealsignature[sigcount] = false;
		stat->loss -= changeofcap;
		stat->lossdeals++;
	}
}

void cdowntrend2(INSTRUMENT *inst, STATINFO *stat, int nday, float enter)
{
	float dup, ddown, dup1, ddown1, changeofcap, fmov8, fmov30, fmov20, enter2, eps = inst->spread;
	float fmov81, f0, f1, f2, df, df30, fmov8_1, fmov30_1, fmov20_1, closep;
	int i = 2, nbars4 = inst->pinstr24h[nday].nbars, nbarss;
	bool signal = false;

	nbarss = 12;

	fmov8 = movh(inst->pinstr24h, nday, 0, inst->Tf, 1, 0.72);
	fmov20 = movh(inst->pinstr24h, nday, 0, 120, 1, 0.91);
	fmov30 = movh(inst->pinstr24h, nday, 0, 30, 1, 0.93);

	dup1 = inst->pinstr24h[nday].open[0];
	ddown1 = dup1;
	float dfmov30;
	//cout<<enter2<<endl;
	while (!signal)
	{
		fmov8 = movh(inst->pinstr24h, nday, i, inst->Tf, 1, 0.71);
		fmov20 = movh(inst->pinstr24h, nday, i, 20, 1, 0.91);
		fmov30 = movh(inst->pinstr24h, nday, i, 30, 1, 0.95);
		{
			if (dup1 < inst->pinstr24h[nday].high[i])
				dup1 = inst->pinstr24h[nday].high[i];
			if (ddown1 > inst->pinstr24h[nday].low[i])
				ddown1 = inst->pinstr24h[nday].low[i];
		}
		if (inst->pinstr24h[nday].close[i] < fmov30 && fdist(inst->pinstr24h[nday].close[i - 1], inst->pinstr24h[nday].close[i - 2], inst->pinstr24h[nday].close[i - 3]) < 3.6 * eps &&
			inst->pinstr24h[nday].close[i] < (fmin3(inst->pinstr24h[nday].close[i - 1], inst->pinstr24h[nday].close[i - 2], inst->pinstr24h[nday].close[i - 3])) && fabs(inst->pinstr24h[nday].close[i] - fmov30) < 2 * eps)
			signal = true;

		i++;
		if (i >= nbarss)
			return;
	}
	enter2 = inst->pinstr24h[nday].open[i];
	inst->sloss = 1.2 * (-inst->pinstr24h[nday].open[i] + fmax3(inst->pinstr24h[nday].high[i - 1], inst->pinstr24h[nday].high[i - 2], inst->pinstr24h[nday].high[i - 3]));
	barcount[i - 1]++;
	dup = enter2;
	ddown = enter2;

	// define post-deal high & low
	signal = false;
	while (i < nbars4)
	{
		if (dup < inst->pinstr24h[nday].high[i])
			dup = inst->pinstr24h[nday].high[i];
		if (ddown > inst->pinstr24h[nday].low[i])
			ddown = inst->pinstr24h[nday].low[i];
		i++;
	}
	// results
	dup = dup - enter2 + inst->spread;
	ddown = -(ddown - enter2 + inst->spread);
	// verify for stoploss
	if (dup >= inst->sloss)
	{
		changeofcap = inst->dx * inst->sloss;
		stat->cap -= changeofcap;
		stat->change -= changeofcap;
		stat->dealsignature[sigcount] = false;
		stat->loss += changeofcap;
		if (stat->maxloss < changeofcap)
			stat->maxloss = changeofcap;
		stat->lossdeals++;
		return;
	}
	// verify the takeprofit
	if (ddown >= inst->tprofit)
	{
		changeofcap = inst->dx * inst->tprofit;
		stat->cap += changeofcap;
		stat->change += changeofcap;
		stat->dealsignature[sigcount] = true;
		stat->profit += changeofcap;
		if (stat->maxprofit < changeofcap)
			stat->maxprofit = changeofcap;
		stat->profitdeals++;
		return;
	}
	// close at the end of the day
	changeofcap = inst->dx * (enter2 - inst->pinstr24h[nday].close[i - 1] - inst->spread);
	stat->cap += changeofcap;
	if (changeofcap > 0)
	{
		if (stat->maxprofit < changeofcap)
			stat->maxprofit = changeofcap;
		stat->change += changeofcap;
		stat->dealsignature[sigcount] = true;
		stat->profit += changeofcap;
		stat->profitdeals++;
	}
	else
	{
		if (stat->maxloss < changeofcap)
			stat->maxloss = changeofcap;
		stat->change += changeofcap;
		stat->dealsignature[sigcount] = false;
		stat->loss -= changeofcap;
		stat->lossdeals++;
	}
}

void cuptrend(INSTRUMENT *inst, STATINFO *stat, int nday, float enter)
{
	float dup, ddown, changeofcap;

	dup = inst->pinstr[nday].high - enter - inst->spread;
	ddown = inst->pinstr[nday].low - enter - inst->spread;
	{ // deal done
		// verify the stoploss
		if ((-ddown) >= inst->sloss)
		{
			changeofcap = inst->dx * inst->sloss;
			stat->cap -= changeofcap;
			if (stat->maxloss < changeofcap)
				stat->maxloss = changeofcap;
			stat->lossdeals++;
			return;
		}
		// verify for tprofit
		if (dup >= inst->tprofit)
		{
			changeofcap = inst->dx * inst->tprofit;
			stat->cap += changeofcap;
			if (stat->maxprofit < changeofcap)
				stat->maxprofit = changeofcap;
			stat->profitdeals++;
			return;
		}
		// close at the end of the day
		changeofcap = inst->dx * (inst->pinstr[nday].close - enter - inst->spread);
		stat->cap += changeofcap;
		if (changeofcap > 0)
		{
			if (stat->maxprofit < changeofcap)
				stat->maxprofit = changeofcap;
			stat->profitdeals++;
		}
		else
		{
			if (stat->maxloss < changeofcap)
				stat->maxloss = changeofcap;
			stat->lossdeals++;
		}
	}
}

void cdowntrend(INSTRUMENT *inst, STATINFO *stat, int nday, float enter)
{
	float dup, ddown, changeofcap;

	dup = inst->pinstr[nday].high - enter + inst->spread;
	;
	ddown = enter - inst->pinstr[nday].low - inst->spread;
	{ // deal done
		// verify the stoploss
		if ((dup) >= inst->sloss)
		{
			changeofcap = inst->dx * inst->sloss;
			stat->cap -= changeofcap;
			if (stat->maxloss < changeofcap)
				stat->maxloss = changeofcap;
			stat->lossdeals++;
			return;
		}
		// verify for takeprofit
		if (ddown >= inst->tprofit)
		{
			changeofcap = inst->dx * inst->tprofit;
			stat->cap += changeofcap;
			if (stat->maxprofit < changeofcap)
				stat->maxprofit = changeofcap;
			stat->profitdeals++;
			return;
		}
		// close at the end of the day
		changeofcap = inst->dx * (enter - inst->pinstr[nday].close - inst->spread);
		stat->cap += changeofcap;
		if (changeofcap > 0)
		{
			if (stat->maxprofit < changeofcap)
				stat->maxprofit = changeofcap;
			stat->profitdeals++;
		}
		else
		{
			if (stat->maxloss < changeofcap)
				stat->maxloss = changeofcap;
			stat->lossdeals++;
		}
	}
}

#define PRINT_STAT 1
#define N_INSTR 12

int main(int argc, char *argv[])
{
	int ninstr = 12;
	int dealinstr = 6;
	int zstrategy = 0;
	int instrarray[10] = {1, 2, 3, 4, 9, 10, 3, 5, 9, 8}; // define here set of instruments
	int maxsizeh = 365 * 120;
	int maxsized = 365 * 5;
	int startdate = 1162242000;
	int enddate = 1213995600;
	int longtime = enddate - startdate;
	int dlongtime = longtime / 86400 + 10;
	int dlongtime24 = longtime / 3600 + 10;
	INSTRUMENT instr[N_INSTR];
	STATINFO statistics;

	char *instrnames[12] = {
				"audcadh", 
				"audusdh", 
				"nzdusdh", 
				"eurchfh",
				"chfjpyh", 
				"eurjpyh", 
				"audjpyh", 
				"gbpjpyh",
				"cadusdh", 
				"cadjpyh", 
				"eurcadh", 
				"gbpusdh"
				};

	float sdts[60] = {
			  0.001, 1000.0, 0.008, 0.008, 7, 
			  0.0005, 1000.0, 0.005, 0.004, 7,
			  0.0004, 1000.0, 0.004, 0.004, 7, 
  			  0.0004, 1000.0, 0.005, 0.003, 8,
			  0.06, 10.0, 0.4, 0.4, 6, 
			  0.04, 10.0, 0.6, 0.4, 6,
			  0.12, 10.0, 0.8, 0.8, 6, 
			  0.08, 10.0, 0.9, 0.6, 6,
			  0.0004, 1000.0, 0.007, 0.004, 7, 
			  0.06, 10.0, 0.9, 0.8, 6,
			  0.0003, 1000.0, 0.006, 0.004, 8, 
			  0.0010, 1000.0, 0.007, 0.007, 8
			 };
	BAR *instrh;
	BAR *instrd;
	int sizeh, sized, i, j, k;
	instrh = (BAR *)SOR_malloc(maxsizeh * sizeof(BAR));
	j = 0;
	for (i = 0; i < ninstr; i++)
	{
		instr[i].ID = i;
		instr[i].spread = sdts[j++];
		instr[i].dx = sdts[j++];
		instr[i].tprofit = sdts[j++];
		instr[i].sloss = sdts[j++];
		instr[i].Tf = sdts[j++];
		if (!LoadData(instrnames[i], instrh, &sizeh))
		{
			printf("loading error of %s\n", instrnames[i]);
			return EXIT_SUCCESS;
		}
		instr[i].pinstr = (BAR *)SOR_malloc(dlongtime * sizeof(BAR));
		instr[i].pinstr24h = (BAR24 *)SOR_malloc(dlongtime24 * sizeof(BAR24));
		cout << instrnames[i] << " loaded" << endl;
		convh2d(instrh, instr[i].pinstr, startdate, enddate, &instr[i].length);
		convh224h(instrh, instr[i].pinstr24h, startdate, enddate);
	}
	cout << "Bars have been sucessfully Converted" << endl;

	// simulation section
	statistics.profitdeals = 0;
	statistics.lossdeals = 0;
	statistics.maxprofit = 0;
	statistics.maxloss = 0;
	statistics.profit = 0;
	statistics.loss = 0;
	statistics.lostalldays = 0;
	statistics.gainalldays = 0;
	statistics.mediumdays = 0;
	statistics.startcap = 100.00;
	statistics.cap = statistics.startcap;
	statistics.maxcap = statistics.cap;
	statistics.lastcap = statistics.cap;
	statistics.maxfall = 0.0;
	for (i = 0; i < 6; i++)
		barcount[i] = 0;

	int P = 5, PP = 20, curinstr;
	float fmov = 0, fmov1, fmov2, fmovd, fmov1d;
	float deal;

	// 4h-trading section

	for (i = P + 2; i < dlongtime; i++)
	{
		if (i == 650 || i == 1015 || i == 426)
			i++;
		statistics.change = 0.0;
		for (k = 0; k < dealinstr; k++)
		{
			sigcount = k;
			statistics.dealsignature[k] = false;
			curinstr = instrarray[k];
			if (instr[curinstr].length >= i)
			{
				fmov = mov(instr[curinstr].pinstr, i - 1, P, 1, 0.71);
				fmov1 = mov(instr[curinstr].pinstr, i - 1, PP, 1, 0.95);
				deal = fmov;
				if (k >= zstrategy)
					cuptrend2(&instr[curinstr], &statistics, i, deal);

			}
			statistics.totaldeals = statistics.profitdeals + statistics.lossdeals;
		}
		if (PRINT_STAT)
		{
			printf("day%d ", i);
			if (statistics.change != 0)
				for (k = 0; k < dealinstr; k++)
					if (statistics.dealsignature[k])
					{
						printf("+");
					}
					else
					{
						printf("-");
					};
			printf(" change: %f    cap: %f\n", statistics.change, statistics.cap);
		}
		float deltac;
		if (statistics.cap > statistics.maxcap)
		{
			statistics.maxcap = statistics.cap;
		}
		else
		{
			deltac = statistics.maxcap - statistics.cap;
			if (deltac > statistics.maxfall)
				statistics.maxfall = deltac;
		}
		bool flag1;
		flag1 = true;
		if (statistics.change != 0)
		{
			for (k = 0; k < dealinstr; k++)
				if (!statistics.dealsignature[k])
					flag1 = false;
			if (flag1)
			{
				statistics.gainalldays++;
			}
			else
			{
				flag1 = true;
				for (k = 0; k < dealinstr; k++)
					if (statistics.dealsignature[k])
						flag1 = false;
				if (flag1)
				{
					statistics.lostalldays++;
				}
				else
				{
					statistics.mediumdays++;
				}
			}
		}
	}
	if (statistics.totaldeals == 0)
		statistics.totaldeals = 1;

	printf("-------------------------------------------------------------------------\n");
	printf("---  Trade Instruments  ---\n");
	for (k = 0; k < dealinstr; k++)
		printf("%s\n", instrnames[instrarray[k]]);

	printf("-------------------------------------------------------------------------\n");

	printf(" start capital $%f   end  capital: $%f\n", statistics.startcap, statistics.cap);
	printf(" total deals: %d\n", statistics.totaldeals);
	printf(" all-profit days: %d all-lost days: %d mixed profit days: %d\n", statistics.gainalldays, statistics.lostalldays, statistics.mediumdays);
	printf(" profit deals: %d  (%f) loss deals: %d (%f)\n", statistics.profitdeals, (float)100 * statistics.profitdeals / statistics.totaldeals, statistics.lossdeals, (float)100 * statistics.lossdeals / statistics.totaldeals);
	printf(" profit: $%f  loss:$%f\n", statistics.profit, statistics.loss);
	printf(" avr profit: $%f avr loss:$%f  avrProfit/avrLoss=%f\n", statistics.profit / statistics.profitdeals, statistics.loss / statistics.lossdeals,
		   statistics.profit * statistics.lossdeals / (statistics.profitdeals * statistics.loss));
	printf(" maxcap $%f   maxfall: $%f\n", statistics.maxcap, statistics.maxfall);
	printf(" maxgain %f   maxloss: %f\n", statistics.maxprofit, statistics.maxloss);
	for (i = 0; i < 24; i++)
	{
		printf("  %d ", barcount[i]);
	}
	printf("\n");
	return EXIT_SUCCESS;
}

