/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
** Copyright (C) 2002-2013 Sourcefire, Inc.
** Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* sp_icmp_seq_check
 *
 * Purpose:
 *
 * Test the Sequence number field of ICMP ECHO and ECHO_REPLY packets for
 * specified values.  This is useful for detecting TFN attacks, amongst others.
 *
 * Arguments:
 *
 * The ICMP Seq plugin takes a number as an option argument.
 *
 * Effect:
 *
 * Tests ICMP ECHO and ECHO_REPLY packet Seq field values and returns a
 * "positive" detection result (i.e. passthrough) upon a value match.
 *
 * Comments:
 *
 * This plugin was developed to detect TFN distributed attacks.
 *
 */

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ctype.h>

#include "snort_types.h"
#include "detection/treenodes.h"
#include "protocols/packet.h"
#include "parser.h"
#include "snort_debug.h"
#include "util.h"
#include "snort.h"
#include "profiler.h"
#include "detection/fpdetect.h"
#include "sfhashfcn.h"
#include "detection/detection_defines.h"
#include "framework/ips_option.h"
#include "framework/parameter.h"
#include "framework/module.h"
#include "framework/range.h"

static const char* s_name = "icmp_seq";

static THREAD_LOCAL ProfileStats icmpSeqPerfStats;

class IcmpSeqOption : public IpsOption
{
public:
    IcmpSeqOption(const RangeCheck& c) :
        IpsOption(s_name)
    { config = c; };

    uint32_t hash() const;
    bool operator==(const IpsOption&) const;

    int eval(Cursor&, Packet*);

private:
    RangeCheck config;
};

//-------------------------------------------------------------------------
// class methods
//-------------------------------------------------------------------------

uint32_t IcmpSeqOption::hash() const
{
    uint32_t a,b,c;

    a = config.op;
    b = config.min;
    c = config.max;

    mix_str(a,b,c,get_name());
    final(a,b,c);

    return c;
}

bool IcmpSeqOption::operator==(const IpsOption& ips) const
{
    if ( strcmp(get_name(), ips.get_name()) )
        return false;

    IcmpSeqOption& rhs = (IcmpSeqOption&)ips;
    return ( config == rhs.config );

    return false;
}

int IcmpSeqOption::eval(Cursor&, Packet *p)
{
    PROFILE_VARS;

    if(!p->icmph)
        return DETECTION_OPTION_NO_MATCH;

    PREPROC_PROFILE_START(icmpSeqPerfStats);

    if ( (p->icmph->type == ICMP_ECHO ||
          p->icmph->type == ICMP_ECHOREPLY) ||
        ((uint16_t)p->icmph->type == icmp6::Icmp6Types::ECHO ||
         (uint16_t)p->icmph->type == icmp6::Icmp6Types::REPLY) )
    {
        if ( config.eval(p->icmph->s_icmp_seq) )
        {
            PREPROC_PROFILE_END(icmpSeqPerfStats);
            return DETECTION_OPTION_MATCH;
        }
    }
    PREPROC_PROFILE_END(icmpSeqPerfStats);
    return DETECTION_OPTION_NO_MATCH;
}

//-------------------------------------------------------------------------
// module
//-------------------------------------------------------------------------

static const Parameter icmp_id_params[] =
{
    { "*range", Parameter::PT_STRING, nullptr, nullptr,
      "check if packet payload size is min<>max | <max | >min" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

class IcmpSeqModule : public Module
{
public:
    IcmpSeqModule() : Module(s_name, icmp_id_params) { };

    bool begin(const char*, int, SnortConfig*);
    bool set(const char*, Value&, SnortConfig*);

    ProfileStats* get_profile() const
    { return &icmpSeqPerfStats; };

    RangeCheck data;
};

bool IcmpSeqModule::begin(const char*, int, SnortConfig*)
{
    data.init();
    return true;
}

bool IcmpSeqModule::set(const char*, Value& v, SnortConfig*)
{
    if ( !v.is("*range") )
        return false;

    return data.parse(v.get_string());
}

//-------------------------------------------------------------------------
// api methods
//-------------------------------------------------------------------------

static Module* mod_ctor()
{
    return new IcmpSeqModule;
}

static void mod_dtor(Module* m)
{
    delete m;
}

static IpsOption* icmp_seq_ctor(Module* p, OptTreeNode*)
{
    IcmpSeqModule* m = (IcmpSeqModule*)p;
    return new IcmpSeqOption(m->data);
}

static void icmp_seq_dtor(IpsOption* p)
{
    delete p;
}

static const IpsApi icmp_seq_api =
{
    {
        PT_IPS_OPTION,
        s_name,
        IPSAPI_PLUGIN_V0,
        0,
        mod_ctor,
        mod_dtor
    },
    OPT_TYPE_DETECTION,
    1, PROTO_BIT__ICMP,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    icmp_seq_ctor,
    icmp_seq_dtor,
    nullptr
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &icmp_seq_api.base,
    nullptr
};
#else
const BaseApi* ips_icmp_seq = &icmp_seq_api.base;
#endif

