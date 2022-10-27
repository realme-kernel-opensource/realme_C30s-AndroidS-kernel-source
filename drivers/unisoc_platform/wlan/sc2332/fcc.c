/*
* SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: GPL-2.0
*
* Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License
* as published by the Free Software Foundation.
*/

#include <misc/wcn_integrate_platform.h>
#include "common/common.h"
#include "cmdevt.h"
#include "fcc.h"

static struct fcc_power_bo g_fcc_power_table[MAC_FCC_COUNTRY_NUM] = {
	{
		.num = 11,
		.power_backoff = {
			/* subtype, value, mode, channel */
			{1, 9, 1, 1},
                        {1, 11, 1, 2},
			{1, 11, 1, 9},
			{1, 11, 1, 10},
                        {1, 9, 1, 11},
                        {1, 9, 2, 1},
                        {1, 11, 2, 2},
                        {1, 13, 2, 8},
			{1, 11, 2, 9},
			{1, 10, 2, 10},
                        {1, 8, 2, 11},
		},
	},
};

static struct fcc_power_bo g_ce_power_table[MAC_FCC_COUNTRY_NUM] = {
	{
		.num = 13,
		.power_backoff = {
			/* subtype, value, mode, channel */
			{1, 13, 0, 1},
			{1, 16, 0, 2},
			{1, 16, 0, 3},
			{1, 16, 0, 4},
			{1, 16, 0, 5},
			{1, 16, 0, 6},
			{1, 13, 0, 7},
			{1, 16, 0, 8},
			{1, 16, 0, 9},
			{1, 16, 0, 10},
			{1, 16, 0, 11},
			{1, 16, 0, 12},
			{1, 13, 0, 13},
		},
	},
};

static struct fcc_power_bo g_fcc_power_table_22[MAC_FCC_COUNTRY_NUM] = {
	{
		.num = 11,
		.power_backoff = {
			/* subtype, value, mode, channel */
			{1, 9, 1, 1},
                        {1, 11, 1, 2},
			{1, 11, 1, 9},
			{1, 11, 1, 10},
                        {1, 9, 1, 11},
                        {1, 9, 2, 1},
                        {1, 11, 2, 2},
                        {1, 13, 2, 8},
			{1, 11, 2, 9},
			{1, 10, 2, 10},
                        {1, 8, 2, 11},
		},
	},
};

static struct fcc_power_bo g_ce_power_table_22[MAC_FCC_COUNTRY_NUM] = {
	{
		.num = 13,
		.power_backoff = {
			/* subtype, value, mode, channel */
			{1, 13, 0, 1},
			{1, 16, 0, 2},
			{1, 16, 0, 3},
			{1, 16, 0, 4},
			{1, 16, 0, 5},
			{1, 16, 0, 6},
			{1, 13, 0, 7},
			{1, 16, 0, 8},
			{1, 16, 0, 9},
			{1, 16, 0, 10},
			{1, 16, 0, 11},
			{1, 16, 0, 12},
			{1, 13, 0, 13},
		},
	},
};

//HW reuqest add by alex.kou
struct country_type {
	char country[3];
};

static struct country_type fcc_country_list[] = {
	{"MX"}, {"CO"}, {"PE"}, {"EC"}, {"CL"}, {"BO"},
};

static struct country_type ce_country_list[] = {
	{"IN"}, {"BD"}, {"ID"}, {"PK"}, {"ES"}, {"IT"}, {"FR"}, {"GB"}, {"DE"}, {"PT"},
	{"NL"}, {"BE"}, {"FI"}, {"PL"}, {"CZ"}, {"SK"}, {"GR"}, {"HU"}, {"AT"}, {"HR"},
	{"BG"}, {"RO"}, {"SI"}, {"RS"}, {"ME"}, {"MK"}, {"NP"}, {"MM"}, {"TW"}, {"LK"},
	{"VN"}, {"PH"}, {"MY"}, {"SG"}, {"KH"}, {"TH"}, {"RU"}, {"BY"}, {"UA"}, {"KZ"},
	{"UZ"}, {"IQ"}, {"JO"}, {"IL"}, {"MA"}, {"TN"}, {"DZ"}, {"LY"}, {"ZA"}, {"AZ"},
	{"GE"}, {"AE"}, {"SA"}, {"KE"}, {"EG"}, {"TR"}, {"BR"},
};

bool is_fcc_country(const char *alpha2){
	int i = 0;

	for(i=0; i< ARRAY_SIZE(fcc_country_list); i++){
		if(fcc_country_list[i].country[0] == alpha2[0] &&
				fcc_country_list[i].country[1] == alpha2[1] ){

			return true;
		}
	}

	return false;
}

bool is_ce_country(const char *alpha2){
	int i = 0;

	for(i=0; i< ARRAY_SIZE(ce_country_list); i++){
		if(ce_country_list[i].country[0] == alpha2[0] &&
				ce_country_list[i].country[1] == alpha2[1] ){

			return true;
		}
	}

	return false;
}

void sc2332_fcc_match_country(struct sprd_priv *priv, const char *alpha2)
{
	int index, i;
	bool found_fcc = false;
	struct fcc_power_bo *fcc_power_table = NULL;
	struct fcc_power_bo *ce_power_table = NULL;

	if(wcn_get_aon_chip_id() == WCN_SHARKL3_CHIP){
		fcc_power_table = g_fcc_power_table;
		ce_power_table = g_ce_power_table;
	}else if(wcn_get_aon_chip_id() == WCN_SHARKL3_CHIP_22NM){
		fcc_power_table = g_fcc_power_table_22;
		ce_power_table = g_ce_power_table_22;
	}else {
		fcc_power_table = g_fcc_power_table;
		ce_power_table = g_ce_power_table;
	}

	if(is_fcc_country(alpha2)){
		found_fcc = true;
		pr_info("need set fcc power\n");
		i = 0;
		for (index = 0; index < fcc_power_table[i].num; index++) {
			sc2332_set_power_backoff(priv, NULL,
					fcc_power_table[i].power_backoff[index].sub_type,
					fcc_power_table[i].power_backoff[index].value,
					fcc_power_table[i].power_backoff[index].mode,
					fcc_power_table[i].power_backoff[index].channel);
		}
	}else if(is_ce_country(alpha2)){
		found_fcc = true;
		pr_info("need set ce power\n");
		i = 0;
		for (index = 0; index < ce_power_table[i].num; index++) {
			sc2332_set_power_backoff(priv, NULL,
					ce_power_table[i].power_backoff[index].sub_type,
					ce_power_table[i].power_backoff[index].value,
					ce_power_table[i].power_backoff[index].mode,
					ce_power_table[i].power_backoff[index].channel);
		}
	}

	if (!found_fcc) {
		pr_info("not fcc country,need reset fcc power\n");
		sc2332_set_power_backoff(priv, NULL, 0, 0, 0, 1);
	}
}
